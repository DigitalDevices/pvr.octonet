#include "rtsp_client.hpp"
#include <algorithm>
#include <cctype>
#include <iterator>
#include "Socket.h"
#include "client.h"
#include <p8-platform/util/util.h>
#include <kodi/libXBMC_addon.h>
#include <cstring>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
#define strtok_r strtok_s
#define strncasecmp _strnicmp

int vasprintf(char **sptr, char *fmt, va_list argv) {
	int wanted = vsnprintf(*sptr = NULL, 0, fmt, argv);
	if((wanted < 0) || ((*sptr = (char *)malloc(1 + wanted)) == NULL))
		return -1;
	return vsprintf(*sptr, fmt, argv);
}

int asprintf(char **sptr, char *fmt, ...) {
	int retval;
	va_list argv;
	va_start(argv, fmt);
	retval = vasprintf(sptr, fmt, argv);
	va_end(argv);
	return retval;
}
#endif

#define RTSP_DEFAULT_PORT 554
#define RTSP_RECEIVE_BUFFER 2048
#define RTP_HEADER_SIZE 12
#define VLEN 100
#define KEEPALIVE_INTERVAL 60
#define KEEPALIVE_MARGIN 5
#define UDP_ADDRESS_LEN 16

using namespace std;
using namespace ADDON;
using namespace OCTO;

enum rtsp_state {
	RTSP_IDLE,
	RTSP_DESCRIBE,
	RTSP_SETUP,
	RTSP_PLAY,
	RTSP_RUNNING
};

enum rtsp_result {
	RTSP_RESULT_OK = 200,
};

struct rtsp_client {
	char *content_base;
	char *control;
	char session_id[64];
	uint16_t stream_id;
	int keepalive_interval;

	char udp_address[UDP_ADDRESS_LEN];
	uint16_t udp_port;

	Socket tcp_sock;
	Socket udp_sock;

	enum rtsp_state state;
	int cseq;

	size_t fifo_size;
	uint16_t last_seq_nr;
};

struct url {
	string protocol;
	string host;
	int port;
	string path;
};

static rtsp_client *rtsp = NULL;

static url parse_url(const std::string& str) {
	static const string prot_end = "://";
	static const string host_end = "/";
	url result;

	string::const_iterator begin = str.begin();
	string::const_iterator end = search(begin, str.end(), prot_end.begin(), prot_end.end());
	result.protocol.reserve(distance(begin, end));
	transform(begin, end, back_inserter(result.protocol), ::tolower);
	advance(end, prot_end.size());
	begin = end;

	end = search(begin, str.end(), host_end.begin(), host_end.end());
	result.host.reserve(distance(begin, end));
	transform(begin, end, back_inserter(result.host), ::tolower);
	advance(end, host_end.size());
	begin = end;

	result.port = RTSP_DEFAULT_PORT;

	result.path.reserve(distance(begin, str.end()));
	transform(begin, str.end(), back_inserter(result.path), ::tolower);

	return result;
}

static int tcp_sock_read_line(string &line) {
	static string buf;

	while(true) {
		string::size_type pos = buf.find("\r\n");
		if(pos != string::npos) {
			line = buf.substr(0, pos);
			buf.erase(0, pos + 2);
			return 0;
		}

		char tmp_buf[2048];
		int size = rtsp->tcp_sock.receive(tmp_buf, sizeof(tmp_buf), 1);
		if(size <= 0) {
			return 1;
		}

		buf.append(&tmp_buf[0], &tmp_buf[size]);
	}
}

static string compose_url(const url& u)
{
	stringstream res;
	res << u.protocol << "://" << u.host;
	if (u.port > 0)
		res << ":" << u.port;
	res << "/" << u.path;

	return res.str();
}

static void parse_session(char *request_line, char *session, unsigned max, int *timeout) {
	char *state;
	char *tok;

	tok = strtok_r(request_line, ";", &state);
	if (tok == NULL)
		return;
	strncpy(session, tok, min(strlen(tok), (size_t)(max - 1)));

	while ((tok = strtok_r(NULL, ";", &state)) != NULL) {
		if (strncmp(tok, "timeout=", 8) == 0) {
			*timeout = atoi(tok + 8);
			if (*timeout > 5)
				*timeout -= KEEPALIVE_MARGIN;
			else if (*timeout > 0)
				*timeout = 1;
		}
	}
}

static int parse_port(char *str, uint16_t *port)
{
	int p = atoi(str);
	if (p < 0 || p > UINT16_MAX)
		return -1;

	*port = p;

	return 0;
}

static int parse_transport(char *request_line) {
	char *state;
	char *tok;
	int err;

	tok = strtok_r(request_line, ";", &state);
	if (tok == NULL || strncmp(tok, "RTP/AVP", 7) != 0)
		return -1;

	tok = strtok_r(NULL, ";", &state);
	if (tok == NULL || strncmp(tok, "multicast", 9) != 0)
		return 0;

	while ((tok = strtok_r(NULL, ";", &state)) != NULL) {
		if (strncmp(tok, "destination=", 12) == 0) {
			strncpy(rtsp->udp_address, tok + 12, min(strlen(tok + 12), (size_t)(UDP_ADDRESS_LEN - 1)));
		} else if (strncmp(tok, "port=", 5) == 0) {
			char port[6];
			char *end;

			memset(port, 0x00, 6);
			strncpy(port, tok + 5, min(strlen(tok + 5), (size_t)5));
			if ((end = strstr(port, "-")) != NULL)
				*end = '\0';
			err = parse_port(port, &rtsp->udp_port);
			if (err)
				return err;
		}
	}

	return 0;
}

#define skip_whitespace(x) while(*x == ' ') x++
static enum rtsp_result rtsp_handle() {
	uint8_t buffer[512];
	int rtsp_result = 0;
	bool have_header = false;
	size_t content_length = 0;
	size_t read = 0;
	char *in, *val;
	string in_str;

	/* Parse header */
	while (!have_header) {
		if (tcp_sock_read_line(in_str) < 0)
			break;
		in = const_cast<char *>(in_str.c_str());

		if (strncmp(in, "RTSP/1.0 ", 9) == 0) {
			rtsp_result = atoi(in + 9);
		} else if (strncmp(in, "Content-Base:", 13) == 0) {
			free(rtsp->content_base);

			val = in + 13;
			skip_whitespace(val);

			rtsp->content_base = strdup(val);
		} else if (strncmp(in, "Content-Length:", 15) == 0) {
			val = in + 16;
			skip_whitespace(val);

			content_length = atoi(val);
		} else if (strncmp("Session:", in, 8) == 0) {
			val = in + 8;
			skip_whitespace(val);

			parse_session(val, rtsp->session_id, 64, &rtsp->keepalive_interval);
		} else if (strncmp("Transport:", in, 10) == 0) {
			val = in + 10;
			skip_whitespace(val);

			if (parse_transport(val) != 0) {
				rtsp_result = -1;
				break;
			}
		} else if (strncmp("com.ses.streamID:", in, 17) == 0) {
			val = in + 17;
			skip_whitespace(val);

			rtsp->stream_id = atoi(val);
		} else if (in[0] == '\0') {
			have_header = true;
		}
	}

	/* Discard further content */
	while (content_length > 0 &&
			(read = rtsp->tcp_sock.receive((char*)buffer, sizeof(buffer), min(sizeof(buffer), content_length))))
		content_length -= read;

	return (enum rtsp_result)rtsp_result;
}

bool rtsp_open(const string& url_str)
{
	string setup_url_str;
	const char *psz_setup_url;
	stringstream setup_ss;
	stringstream play_ss;
	url setup_url;

	rtsp_close();
	rtsp = new rtsp_client();
	if (rtsp == NULL)
		return false;

	kodi->Log(LOG_DEBUG, "try to open '%s'", url_str.c_str());

	url dst = parse_url(url_str);
	kodi->Log(LOG_DEBUG, "connect to host '%s'", dst.host.c_str());

	if(!rtsp->tcp_sock.connect(dst.host, dst.port)) {
		kodi->Log(LOG_ERROR, "Failed to connect to RTSP server %s:%d", dst.host.c_str(), dst.port);
		goto error;
	}

	// TODO: tcp keep alive?

	if (asprintf(&rtsp->content_base, "rtsp://%s:%d/", dst.host.c_str(),
				dst.port) < 0) {
		rtsp->content_base = NULL;
		goto error;
	}

	rtsp->last_seq_nr = 0;
	rtsp->keepalive_interval = (KEEPALIVE_INTERVAL - KEEPALIVE_MARGIN);

	setup_url = dst;

	// reverse the satip protocol trick, as SAT>IP believes to be RTSP
	if (!strncasecmp(setup_url.protocol.c_str(), "satip", 5)) {
		setup_url.protocol = "rtsp";
	}

	setup_url_str = compose_url(setup_url);
	psz_setup_url = setup_url_str.c_str();

	// TODO: Find available port
	rtsp->udp_sock = Socket(af_inet, pf_inet, sock_dgram, udp);
	rtsp->udp_port = 6785;
	if(!rtsp->udp_sock.bind(rtsp->udp_port)) {
		goto error;
	}

	setup_ss << "SETUP " << setup_url_str<< " RTSP/1.0\r\n";
	setup_ss << "CSeq: " << rtsp->cseq++ << "\r\n";
	setup_ss << "Transport: RTP/AVP;unicast;client_port=" << rtsp->udp_port << "-" << (rtsp->udp_port + 1) << "\r\n\r\n";
	rtsp->tcp_sock.send(setup_ss.str());

	if (rtsp_handle() != RTSP_RESULT_OK) {
		kodi->Log(LOG_ERROR, "Failed to setup RTSP session");
		goto error;
	}

	if (asprintf(&rtsp->control, "%sstream=%d", rtsp->content_base, rtsp->stream_id) < 0) {
		rtsp->control = NULL;
		goto error;
	}

	play_ss << "PLAY " << rtsp->control << " RTSP/1.0\r\n";
	play_ss << "CSeq: " << rtsp->cseq++ << "\r\n";
	play_ss << "Session: " << rtsp->session_id << "\r\n\r\n";
	rtsp->tcp_sock.send(play_ss.str());

	if (rtsp_handle() != RTSP_RESULT_OK) {
		kodi->Log(LOG_ERROR, "Failed to play RTSP session");
		goto error;
	}

	return true;

error:
	rtsp_close();
	return false;
}

int rtsp_read(void *buf, unsigned buf_size) {
	sockaddr addr;
	socklen_t addr_len = sizeof(addr);
	int ret = rtsp->udp_sock.recvfrom((char *)buf, buf_size, (sockaddr *)&addr, &addr_len);

	// TODO: check ip

	return ret;
}

static void rtsp_teardown() {
	if(!rtsp->tcp_sock.is_valid()) {
		return;
	}

	if (rtsp->session_id[0] > 0) {
		char *msg;
		int len;
		stringstream ss;

		rtsp->udp_sock.close();

		ss << "TEARDOWN " << rtsp->control << " RTSP/1.0\r\n";
		ss << "CSeq: " << rtsp->cseq++ << "\r\n";
		ss << "Session: " << rtsp->session_id << "\r\n\r\n";
		rtsp->tcp_sock.send(ss.str());

		if (rtsp_handle() != RTSP_RESULT_OK) {
			kodi->Log(LOG_ERROR, "Failed to teardown RTSP session");
			return;
		}
	}
}

void rtsp_close()
{
	if(rtsp) {
		rtsp_teardown();
		rtsp->tcp_sock.close();
		rtsp->udp_sock.close();
		delete rtsp;
		rtsp = NULL;
	}
}
