/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Socket.h"

#include <cstdio>
#include <kodi/General.h>
#include <string>

using namespace std;

namespace OCTO
{

/* Master defines for client control */
#define RECEIVE_TIMEOUT 6 //sec

Socket::Socket(const enum SocketFamily family,
               const enum SocketDomain domain,
               const enum SocketType type,
               const enum SocketProtocol protocol)
{
  m_sd = INVALID_SOCKET;
  m_family = family;
  m_domain = domain;
  m_type = type;
  m_protocol = protocol;
  m_port = 0;
  memset(&m_sockaddr, 0, sizeof(m_sockaddr));
}


Socket::Socket()
{
  // Default constructor, default settings
  m_sd = INVALID_SOCKET;
  m_family = af_inet;
  m_domain = pf_inet;
  m_type = sock_stream;
  m_protocol = tcp;
  m_port = 0;
  memset(&m_sockaddr, 0, sizeof(m_sockaddr));
}


Socket::~Socket()
{
  close();
  osCleanup();
}

bool Socket::setHostname(const std::string& host)
{
  m_hostname = host;
  return true;
}

bool Socket::close()
{
  if (is_valid())
  {
    if (m_sd != SOCKET_ERROR)
      closesocket(m_sd);
    m_sd = INVALID_SOCKET;
    return true;
  }
  return false;
}

bool Socket::create()
{
  close();

  if (!osInit())
  {
    return false;
  }

  return true;
}


bool Socket::bind(const unsigned short port)
{

  if (is_valid())
  {
    close();
  }

  m_sd = socket(m_family, m_type, m_protocol);
  m_port = port;
  m_sockaddr.sin_family = (sa_family_t)m_family;
  m_sockaddr.sin_addr.s_addr = INADDR_ANY; //listen to all
  m_sockaddr.sin_port = htons(m_port);

  int bind_return = ::bind(m_sd, (sockaddr*)(&m_sockaddr), sizeof(m_sockaddr));

  if (bind_return == -1)
  {
    errormessage(getLastError(), "Socket::bind");
    return false;
  }

  return true;
}


bool Socket::listen() const
{

  if (!is_valid())
  {
    return false;
  }

  int listen_return = ::listen(m_sd, SOMAXCONN);
  //This is defined as 5 in winsock.h, and 0x7FFFFFFF in winsock2.h.
  //linux 128//MAXCONNECTIONS =1

  if (listen_return == -1)
  {
    errormessage(getLastError(), "Socket::listen");
    return false;
  }

  return true;
}


bool Socket::accept(Socket& new_socket) const
{
  if (!is_valid())
  {
    return false;
  }

  socklen_t addr_length = sizeof(m_sockaddr);
  new_socket.m_sd =
      ::accept(m_sd, const_cast<sockaddr*>((const sockaddr*)&m_sockaddr), &addr_length);

#ifdef TARGET_WINDOWS
  if (new_socket.m_sd == INVALID_SOCKET)
#else
  if (new_socket.m_sd <= 0)
#endif
  {
    errormessage(getLastError(), "Socket::accept");
    return false;
  }

  return true;
}


int Socket::send(const std::string& data)
{
  return Socket::send((const char*)data.c_str(), (const unsigned int)data.size());
}


int Socket::send(const char* data, const unsigned int len)
{
  fd_set set_w, set_e;
  struct timeval tv;
  int result;

  if (!is_valid())
  {
    return 0;
  }

  // fill with new data
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&set_w);
  FD_ZERO(&set_e);
  FD_SET(m_sd, &set_w);
  FD_SET(m_sd, &set_e);

  result = select(FD_SETSIZE, &set_w, nullptr, &set_e, &tv);

  if (result < 0)
  {
    kodi::Log(ADDON_LOG_ERROR, "Socket::send  - select failed");
    close();
    return 0;
  }
  if (FD_ISSET(m_sd, &set_w))
  {
    kodi::Log(ADDON_LOG_ERROR, "Socket::send  - failed to send data");
    close();
    return 0;
  }

  int status = ::send(m_sd, data, len, 0);

  if (status == -1)
  {
    errormessage(getLastError(), "Socket::send");
    kodi::Log(ADDON_LOG_ERROR, "Socket::send  - failed to send data");
    close();
    return 0;
  }
  return status;
}


int Socket::sendto(const char* data, unsigned int size, bool sendcompletebuffer)
{
  int sentbytes = 0;
  int i;

  do
  {
    i = ::sendto(m_sd, data, size, 0, (const struct sockaddr*)&m_sockaddr, sizeof(m_sockaddr));

    if (i <= 0)
    {
      errormessage(getLastError(), "Socket::sendto");
      osCleanup();
      return i;
    }
    sentbytes += i;
  } while ((sentbytes < (int)size) && (sendcompletebuffer == true));

  return i;
}


int Socket::receive(std::string& data, unsigned int minpacketsize) const
{
  char* buf = nullptr;
  int status = 0;

  if (!is_valid())
  {
    return 0;
  }

  buf = new char[minpacketsize + 1];
  memset(buf, 0, minpacketsize + 1);

  status = receive(buf, minpacketsize, minpacketsize);

  data = buf;

  delete[] buf;
  return status;
}


//Receive until error or \n
bool Socket::ReadLine(string& line)
{
  fd_set set_r, set_e;
  timeval timeout;
  int retries = 6;
  char buffer[2048];

  if (!is_valid())
    return false;

  while (true)
  {
    size_t pos1 = line.find("\r\n", 0);
    if (pos1 != std::string::npos)
    {
      line.erase(pos1, string::npos);
      return true;
    }

    timeout.tv_sec = RECEIVE_TIMEOUT;
    timeout.tv_usec = 0;

    // fill with new data
    FD_ZERO(&set_r);
    FD_ZERO(&set_e);
    FD_SET(m_sd, &set_r);
    FD_SET(m_sd, &set_e);
    int result = select(FD_SETSIZE, &set_r, nullptr, &set_e, &timeout);

    if (result < 0)
    {
      kodi::Log(ADDON_LOG_DEBUG, "%s: select failed", __func__);
      errormessage(getLastError(), __func__);
      close();
      return false;
    }

    if (result == 0)
    {
      if (retries != 0)
      {
        kodi::Log(ADDON_LOG_DEBUG, "%s: timeout waiting for response, retrying... (%i)", __func__,
                  retries);
        retries--;
        continue;
      }
      else
      {
        kodi::Log(ADDON_LOG_DEBUG, "%s: timeout waiting for response. Aborting after 10 retries.",
                  __func__);
        return false;
      }
    }

    result = recv(m_sd, buffer, sizeof(buffer) - 1, 0);
    if (result < 0)
    {
      kodi::Log(ADDON_LOG_DEBUG, "%s: recv failed", __func__);
      errormessage(getLastError(), __func__);
      close();
      return false;
    }
    buffer[result] = 0;

    line.append(buffer);
  }

  return true;
}


int Socket::receive(std::string& data) const
{
  char buf[MAXRECV + 1];
  int status = 0;

  if (!is_valid())
  {
    return 0;
  }

  memset(buf, 0, MAXRECV + 1);
  status = receive(buf, MAXRECV, 0);
  data = buf;

  return status;
}

int Socket::receive(char* data,
                    const unsigned int buffersize,
                    const unsigned int minpacketsize) const
{
  unsigned int receivedsize = 0;

  if (!is_valid())
  {
    return 0;
  }

  while ((receivedsize <= minpacketsize) && (receivedsize < buffersize))
  {
    int status = ::recv(m_sd, data + receivedsize, (buffersize - receivedsize), 0);

    if (status == SOCKET_ERROR)
    {
      errormessage(getLastError(), "Socket::receive");
      return status;
    }

    receivedsize += status;
  }

  return receivedsize;
}


int Socket::recvfrom(char* data,
                     const int buffersize,
                     struct sockaddr* from,
                     socklen_t* fromlen) const
{
  int status = ::recvfrom(m_sd, data, buffersize, 0, from, fromlen);

  return status;
}


bool Socket::connect(const std::string& host, const unsigned short port)
{
  close();

  if (!setHostname(host))
  {
    kodi::Log(ADDON_LOG_ERROR, "Socket::setHostname(%s) failed.\n", host.c_str());
    return false;
  }
  m_port = port;

  char strPort[15];
  snprintf(strPort, 15, "%hu", port);

  struct addrinfo hints;
  struct addrinfo* result = nullptr;
  struct addrinfo* address = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = m_family;
  hints.ai_socktype = m_type;
  hints.ai_protocol = m_protocol;

  int retval = getaddrinfo(host.c_str(), strPort, &hints, &result);
  if (retval != 0)
  {
    errormessage(getLastError(), "Socket::connect");
    return false;
  }

  for (address = result; address != nullptr; address = address->ai_next)
  {
    // Create the socket
    m_sd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

    if (m_sd == INVALID_SOCKET)
    {
      errormessage(getLastError(), "Socket::create");
      continue;
    }

    int status = ::connect(m_sd, address->ai_addr, address->ai_addrlen);
    if (status == SOCKET_ERROR)
    {
      close();
      continue;
    }

    // We have a conection
    break;
  }

  freeaddrinfo(result);

  if (address == nullptr)
  {
    kodi::Log(ADDON_LOG_ERROR, "Socket::connect %s:%u\n", host.c_str(), port);
    errormessage(getLastError(), "Socket::connect");
    close();
    return false;
  }

  return true;
}

bool Socket::reconnect()
{
  if (is_valid())
  {
    return true;
  }

  return connect(m_hostname, m_port);
}

bool Socket::is_valid() const
{
  return (m_sd != INVALID_SOCKET);
}

#if defined(TARGET_WINDOWS)
bool Socket::set_non_blocking(const bool b)
{
  u_long iMode;

  if (b)
    iMode = 1; // enable non_blocking
  else
    iMode = 0; // disable non_blocking

  if (ioctlsocket(m_sd, FIONBIO, &iMode) == -1)
  {
    kodi::Log(ADDON_LOG_ERROR, "Socket::set_non_blocking - Can't set socket condition to: %i",
              iMode);
    return false;
  }

  return true;
}

void Socket::errormessage(int errnum, const char* functionname) const
{
  const char* errmsg = nullptr;

  switch (errnum)
  {
    case WSANOTINITIALISED:
      errmsg = "A successful WSAStartup call must occur before using this function.";
      break;
    case WSAENETDOWN:
      errmsg = "The network subsystem or the associated service provider has failed";
      break;
    case WSA_NOT_ENOUGH_MEMORY:
      errmsg = "Insufficient memory available";
      break;
    case WSA_INVALID_PARAMETER:
      errmsg = "One or more parameters are invalid";
      break;
    case WSA_OPERATION_ABORTED:
      errmsg = "Overlapped operation aborted";
      break;
    case WSAEINTR:
      errmsg = "Interrupted function call";
      break;
    case WSAEBADF:
      errmsg = "File handle is not valid";
      break;
    case WSAEACCES:
      errmsg = "Permission denied";
      break;
    case WSAEFAULT:
      errmsg = "Bad address";
      break;
    case WSAEINVAL:
      errmsg = "Invalid argument";
      break;
    case WSAENOTSOCK:
      errmsg = "Socket operation on nonsocket";
      break;
    case WSAEDESTADDRREQ:
      errmsg = "Destination address required";
      break;
    case WSAEMSGSIZE:
      errmsg = "Message too long";
      break;
    case WSAEPROTOTYPE:
      errmsg = "Protocol wrong type for socket";
      break;
    case WSAENOPROTOOPT:
      errmsg = "Bad protocol option";
      break;
    case WSAEPFNOSUPPORT:
      errmsg = "Protocol family not supported";
      break;
    case WSAEAFNOSUPPORT:
      errmsg = "Address family not supported by protocol family";
      break;
    case WSAEADDRINUSE:
      errmsg = "Address already in use";
      break;
    case WSAECONNRESET:
      errmsg = "Connection reset by peer";
      break;
    case WSAHOST_NOT_FOUND:
      errmsg = "Authoritative answer host not found";
      break;
    case WSATRY_AGAIN:
      errmsg = "Nonauthoritative host not found, or server failure";
      break;
    case WSAEISCONN:
      errmsg = "Socket is already connected";
      break;
    case WSAETIMEDOUT:
      errmsg = "Connection timed out";
      break;
    case WSAECONNREFUSED:
      errmsg = "Connection refused";
      break;
    case WSANO_DATA:
      errmsg = "Valid name, no data record of requested type";
      break;
    default:
      errmsg = "WSA Error";
  }
  kodi::Log(ADDON_LOG_ERROR, "%s: (Winsock error=%i) %s\n", functionname, errnum, errmsg);
}

int Socket::getLastError() const
{
  return WSAGetLastError();
}

int Socket::win_usage_count = 0; //Declared static in Socket class

bool Socket::osInit()
{
  win_usage_count++;
  // initialize winsock:
  if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0)
  {
    return false;
  }

  WORD wVersionRequested = MAKEWORD(2, 2);

  // check version
  if (m_wsaData.wVersion != wVersionRequested)
  {
    return false;
  }

  return true;
}

void Socket::osCleanup()
{
  win_usage_count--;
  if (win_usage_count == 0)
  {
    WSACleanup();
  }
}

#elif defined TARGET_LINUX || defined TARGET_DARWIN || defined TARGET_FREEBSD
bool Socket::set_non_blocking(const bool b)
{
  int opts;

  opts = fcntl(m_sd, F_GETFL);

  if (opts < 0)
  {
    return false;
  }

  if (b)
    opts = (opts | O_NONBLOCK);
  else
    opts = (opts & ~O_NONBLOCK);

  if (fcntl(m_sd, F_SETFL, opts) == -1)
  {
    kodi::Log(ADDON_LOG_ERROR, "Socket::set_non_blocking - Can't set socket flags to: %i", opts);
    return false;
  }
  return true;
}

void Socket::errormessage(int errnum, const char* functionname) const
{
  const char* errmsg = nullptr;

  switch (errnum)
  {
    case EAGAIN: //same as EWOULDBLOCK
      errmsg = "EAGAIN: The socket is marked non-blocking and the requested operation would block";
      break;
    case EBADF:
      errmsg = "EBADF: An invalid descriptor was specified";
      break;
    case ECONNRESET:
      errmsg = "ECONNRESET: Connection reset by peer";
      break;
    case EDESTADDRREQ:
      errmsg = "EDESTADDRREQ: The socket is not in connection mode and no peer address is set";
      break;
    case EFAULT:
      errmsg = "EFAULT: An invalid userspace address was specified for a parameter";
      break;
    case EINTR:
      errmsg = "EINTR: A signal occurred before data was transmitted";
      break;
    case EINVAL:
      errmsg = "EINVAL: Invalid argument passed";
      break;
    case ENOTSOCK:
      errmsg = "ENOTSOCK: The argument is not a valid socket";
      break;
    case EMSGSIZE:
      errmsg = "EMSGSIZE: The socket requires that message be sent atomically, and the size of the "
               "message to be sent made this impossible";
      break;
    case ENOBUFS:
      errmsg = "ENOBUFS: The output queue for a network interface was full";
      break;
    case ENOMEM:
      errmsg = "ENOMEM: No memory available";
      break;
    case EPIPE:
      errmsg = "EPIPE: The local end has been shut down on a connection oriented socket";
      break;
    case EPROTONOSUPPORT:
      errmsg = "EPROTONOSUPPORT: The protocol type or the specified protocol is not supported "
               "within this domain";
      break;
    case EAFNOSUPPORT:
      errmsg = "EAFNOSUPPORT: The implementation does not support the specified address family";
      break;
    case ENFILE:
      errmsg = "ENFILE: Not enough kernel memory to allocate a new socket structure";
      break;
    case EMFILE:
      errmsg = "EMFILE: Process file table overflow";
      break;
    case EACCES:
      errmsg =
          "EACCES: Permission to create a socket of the specified type and/or protocol is denied";
      break;
    case ECONNREFUSED:
      errmsg = "ECONNREFUSED: A remote host refused to allow the network connection (typically "
               "because it is not running the requested service)";
      break;
    case ENOTCONN:
      errmsg = "ENOTCONN: The socket is associated with a connection-oriented protocol and has not "
               "been connected";
      break;
    //case E:
    //	errmsg = "";
    //	break;
    default:
      break;
  }

  kodi::Log(ADDON_LOG_ERROR, "%s: (errno=%i) %s\n", functionname, errnum, errmsg);
}

int Socket::getLastError() const
{
  return errno;
}

bool Socket::osInit()
{
  // Not needed for Linux
  return true;
}

void Socket::osCleanup()
{
  // Not needed for Linux
}
#endif //TARGET_WINDOWS || TARGET_LINUX || TARGET_DARWIN || TARGET_FREEBSD

} //namespace OCTO
