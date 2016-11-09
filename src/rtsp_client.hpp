#ifndef _RTSP_CLIENT_HPP_
#define _RTSP_CLIENT_HPP_

#include <string>
#include <xbmc_pvr_types.h>

bool rtsp_open(const std::string& url_str);
void rtsp_close();
int rtsp_read(void *buf, unsigned buf_size);
void rtsp_fill_signal_status(PVR_SIGNAL_STATUS& signal_status);

#endif
