/*
 * Copyright (C) 2015 Julian Scheel <julian@jusst.de>
 * Copyright (C) 2015 jusst technologies GmbH
 * Copyright (C) 2015 Digital Devices GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 */

#include "client.h"
#include <xbmc_pvr_dll.h>
#include <libXBMC_addon.h>
#include <p8-platform/util/util.h>
#include <libKODI_guilib.h>

#include "OctonetData.h"
#include "rtsp_client.hpp"

using namespace ADDON;

/* setting variables with defaults */
std::string octonetAddress = "";

/* internal state variables */
ADDON_STATUS addonStatus = ADDON_STATUS_UNKNOWN;
CHelper_libXBMC_addon *kodi = NULL;
CHelper_libXBMC_pvr *pvr = NULL;

OctonetData *data = NULL;

/* KODI Core Addon functions
 * see xbmc_addon_dll.h */

extern "C" {

void ADDON_ReadSettings(void)
{
	char buffer[2048];
	if (kodi->GetSetting("octonetAddress", &buffer))
		octonetAddress = buffer;
}

ADDON_STATUS ADDON_Create(void *callbacks, void* props)
{
	if (callbacks == NULL || props == NULL)
		return ADDON_STATUS_UNKNOWN;

	PVR_PROPERTIES *pvrprops = (PVR_PROPERTIES*)props;
	kodi = new CHelper_libXBMC_addon;
	if (!kodi->RegisterMe(callbacks)) {
		kodi->Log(LOG_ERROR, "%s: Failed to register octonet addon", __func__);
		SAFE_DELETE(kodi);
		return ADDON_STATUS_PERMANENT_FAILURE;
	}

	pvr = new CHelper_libXBMC_pvr;
	if (!pvr->RegisterMe(callbacks)) {
		kodi->Log(LOG_ERROR, "%s: Failed to register octonet pvr addon", __func__);
		SAFE_DELETE(pvr);
		SAFE_DELETE(kodi);
		return ADDON_STATUS_PERMANENT_FAILURE;
	}

	kodi->Log(LOG_DEBUG, "%s: Creating octonet pvr addon", __func__);
	ADDON_ReadSettings();

	data = new OctonetData;

	addonStatus = ADDON_STATUS_OK;
	return addonStatus;
}

void ADDON_Stop() {} /* no-op */

void ADDON_Destroy()
{
	delete pvr;
	delete kodi;
	addonStatus = ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS ADDON_GetStatus()
{
	return addonStatus;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
	/* For simplicity do a full addon restart whenever settings are
	 * changed */
	return ADDON_STATUS_NEED_RESTART;
}

}


/* KODI PVR Addon functions
 * see xbmc_pvr_dll.h */
extern "C"
{

const char* GetPVRAPIVersion(void)
{
	return XBMC_PVR_API_VERSION;
}

const char* GetMininumPVRAPIVersion(void)
{
	return XBMC_PVR_MIN_API_VERSION;
}

const char* GetGUIAPIVersion(void)
{
	return KODI_GUILIB_API_VERSION;
}

const char* GetMininumGUIAPIVersion(void)
{
	return KODI_GUILIB_MIN_API_VERSION;
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES *pCapabilities)
{
	pCapabilities->bSupportsTV = true;
	pCapabilities->bSupportsRadio = true;
	pCapabilities->bSupportsChannelGroups = true;
	pCapabilities->bSupportsEPG = true;

	return PVR_ERROR_NO_ERROR;
}

const char* GetBackendName(void)
{
	return "Digital Devices Octopus NET Client";
}

const char* GetBackendVersion(void)
{
	return XBMC_PVR_API_VERSION;
}

const char* GetConnectionString(void)
{
	return "connected"; // FIXME: translate?
}

PVR_ERROR GetDriveSpace(long long* iTotal, long long* iUsed) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK& menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }

void OnSystemSleep() {
	kodi->Log(LOG_INFO, "Received event: %s", __FUNCTION__);
	// FIXME: Disconnect?
}

void OnSystemWake() {
	kodi->Log(LOG_INFO, "Received event: %s", __FUNCTION__);
	// FIXME:Reconnect?
}

void OnPowerSavingActivated() {}
void OnPowerSavingDeactivated() {}

/* EPG */
PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL& channel, time_t iStart, time_t iEnd)
{
	return data->getEPG(handle, channel, iStart, iEnd);
}

/* Channel groups */
int GetChannelGroupsAmount(void)
{
	return data->getGroupCount();
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
	return data->getGroups(handle, bRadio);
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP& group)
{
	return data->getGroupMembers(handle, group);
}

/* Channels */
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }

int GetChannelsAmount(void)
{
	return data->getChannelCount();
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
	return data->getChannels(handle, bRadio);
}

PVR_ERROR DeleteChannel(const PVR_CHANNEL& channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL& channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL& channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL& channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL& channel) { return PVR_ERROR_NOT_IMPLEMENTED; }

/* Recordings */
int GetRecordingsAmount(bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING& recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING& recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY edl[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetTimersAmount(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetTimers(ADDON_HANDLE handle) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR AddTimer(const PVR_TIMER& timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteTimer(const PVR_TIMER& timer, bool bForceDelete) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER& timer) { return PVR_ERROR_NOT_IMPLEMENTED; }

/* PVR stream handling */
/* entirely unused, as we use standard RTSP+TS mux, which can be handlded by
 * Kodi core */
bool OpenLiveStream(const PVR_CHANNEL& channel) {
	return rtsp_open(data->getName(channel.iUniqueId), data->getUrl(channel.iUniqueId));
}

int ReadLiveStream(unsigned char* pBuffer, unsigned int iBufferSize) {
	return rtsp_read(pBuffer, iBufferSize);
}

void CloseLiveStream(void) {
	rtsp_close();
}

long long SeekLiveStream(long long iPosition, int iWhence) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
bool IsRealTimeStream(void) { return true; }

bool SwitchChannel(const PVR_CHANNEL& channel) {
	CloseLiveStream();
	return OpenLiveStream(channel);
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS& signalStatus) {
	memset(&signalStatus, 0, sizeof(PVR_SIGNAL_STATUS));
	rtsp_fill_signal_status(signalStatus);
	return PVR_ERROR_NO_ERROR;
}

const char* GetLiveStreamURL(const PVR_CHANNEL& channel) { return NULL; }
PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties) { return PVR_ERROR_NOT_IMPLEMENTED; }

/* Recording stream handling */
bool OpenRecordedStream(const PVR_RECORDING& recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char* pBuffer, unsigned int iBufferSize) { return -1; }
long long SeekRecordedStream(long long iPosition, int iWhence) { return -1; }
long long PositionRecordedStream(void) { return -1; }
long long LengthRecordedStream(void) { return -1; }

/* PVR demuxer */
/* entirey unused, as we use TS */
void DemuxReset(void) {}
void DemuxAbort(void) {}
void DemuxFlush(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }

/* Various helper functions */
unsigned int GetChannelSwitchDelay(void) { return 0; }
bool IsTimeshifting(void) { return false; }
bool CanPauseStream() { return false; }
bool CanSeekStream() { return false; }

/* Callbacks */
void PauseStream(bool bPaused) {}
bool SeekTime(double time, bool backwards, double *startpts) { return false; }
void SetSpeed(int speed) {}
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }

time_t GetPlayingTime() { return 0; }
time_t GetBufferTimeStart() { return 0; }
time_t GetBufferTimeEnd() { return 0; }

const char* GetBackendHostname()
{
	return octonetAddress.c_str();
}

}
