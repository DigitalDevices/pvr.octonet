#ifndef _RTSP_CLIENT_HPP_
#define _RTSP_CLIENT_HPP_

#include <string>

bool rtsp_open(const std::string& url_str);
void rtsp_close();
int rtsp_read(void *buf, unsigned buf_size);

#endif
