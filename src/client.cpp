/*
 *  Copyright (C) 2015 Julian Scheel <julian@jusst.de>
 *  Copyright (C) 2015 jusst technologies GmbH
 *  Copyright (C) 2015 Digital Devices GmbH
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 *
 */

#include "client.h"
#include <kodi/xbmc_pvr_dll.h>
#include <kodi/libXBMC_addon.h>
#include <p8-platform/util/util.h>

#include "OctonetData.h"
#include "rtsp_client.hpp"

using namespace ADDON;

/* setting variables with defaults */
std::string octonetAddress = "";

/* internal state variables */
ADDON_STATUS addonStatus = ADDON_STATUS_UNKNOWN;
CHelper_libXBMC_addon *libKodi = NULL;
CHelper_libXBMC_pvr *pvr = NULL;

OctonetData *data = NULL;

/* KODI Core Addon functions
 * see xbmc_addon_dll.h */

extern "C" {

void ADDON_ReadSettings(void)
{
	char buffer[2048];
	if (libKodi->GetSetting("octonetAddress", &buffer))
		octonetAddress = buffer;
}

ADDON_STATUS ADDON_Create(void* callbacks, const char* globalApiVersion, void* props)
{
	if (callbacks == NULL || props == NULL)
		return ADDON_STATUS_UNKNOWN;

	AddonProperties_PVR *pvrprops = (AddonProperties_PVR*)props;
	libKodi = new CHelper_libXBMC_addon;
	if (!libKodi->RegisterMe(callbacks)) {
		libKodi->Log(LOG_ERROR, "%s: Failed to register octonet addon", __func__);
		SAFE_DELETE(libKodi);
		return ADDON_STATUS_PERMANENT_FAILURE;
	}

	pvr = new CHelper_libXBMC_pvr;
	if (!pvr->RegisterMe(callbacks)) {
		libKodi->Log(LOG_ERROR, "%s: Failed to register octonet pvr addon", __func__);
		SAFE_DELETE(pvr);
		SAFE_DELETE(libKodi);
		return ADDON_STATUS_PERMANENT_FAILURE;
	}

	libKodi->Log(LOG_DEBUG, "%s: Creating octonet pvr addon", __func__);
	ADDON_ReadSettings();

	data = new OctonetData;

	addonStatus = ADDON_STATUS_OK;
	return addonStatus;
}

void ADDON_Destroy()
{
	delete pvr;
	delete libKodi;
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

PVR_ERROR GetCapabilities(PVR_ADDON_CAPABILITIES *pCapabilities)
{
	pCapabilities->bSupportsTV = true;
	pCapabilities->bSupportsRadio = true;
	pCapabilities->bSupportsChannelGroups = true;
	pCapabilities->bSupportsEPG = true;
	pCapabilities->bSupportsRecordings = false;
	pCapabilities->bSupportsRecordingsRename = false;
	pCapabilities->bSupportsRecordingsLifetimeChange = false;
	pCapabilities->bSupportsDescrambleInfo = false;

	return PVR_ERROR_NO_ERROR;
}

const char* GetBackendName(void)
{
	return "Digital Devices Octopus NET Client";
}

const char* GetBackendVersion(void)
{
	return STR(OCTONET_VERSION);
}

const char* GetConnectionString(void)
{
	return "connected"; // FIXME: translate?
}

PVR_ERROR GetDriveSpace(long long* iTotal, long long* iUsed) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK& menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }

void OnSystemSleep() {
	libKodi->Log(LOG_INFO, "Received event: %s", __FUNCTION__);
	// FIXME: Disconnect?
}

void OnSystemWake() {
	libKodi->Log(LOG_INFO, "Received event: %s", __FUNCTION__);
	// FIXME:Reconnect?
}

void OnPowerSavingActivated() {}
void OnPowerSavingDeactivated() {}

/* EPG */
PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, int iChannelUid, time_t iStart, time_t iEnd)
{
	return data->getEPG(handle, iChannelUid, iStart, iEnd);
}

PVR_ERROR IsEPGTagRecordable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagPlayable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }

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
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL& channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL& channel) { return PVR_ERROR_NOT_IMPLEMENTED; }

/* Recordings */
int GetRecordingsAmount(bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLifetime(const PVR_RECORDING*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING& recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING& recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY edl[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingSize(const PVR_RECORDING* recording, int64_t* sizeInBytes) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetTimersAmount(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetTimers(ADDON_HANDLE handle) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR AddTimer(const PVR_TIMER& timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteTimer(const PVR_TIMER& timer, bool bForceDelete) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER& timer) { return PVR_ERROR_NOT_IMPLEMENTED; }

/* PVR stream properties handling */
PVR_ERROR GetStreamReadChunkSize(int* chunksize) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagEdl(const EPG_TAG* epgTag, PVR_EDL_ENTRY edl[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }

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
long long LengthLiveStream(void) { return -1; }
bool IsRealTimeStream(void) { return true; }

PVR_ERROR GetSignalStatus(int channelUid, PVR_SIGNAL_STATUS* signalStatus) {
	memset(signalStatus, 0, sizeof(PVR_SIGNAL_STATUS));
	rtsp_fill_signal_status(signalStatus);
	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES *times) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetDescrambleInfo(int, PVR_DESCRAMBLE_INFO*) { return PVR_ERROR_NOT_IMPLEMENTED; }

/* Recording stream handling */
bool OpenRecordedStream(const PVR_RECORDING& recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char* pBuffer, unsigned int iBufferSize) { return -1; }
long long SeekRecordedStream(long long iPosition, int iWhence) { return -1; }
long long LengthRecordedStream(void) { return -1; }

/* PVR demuxer */
/* entirey unused, as we use TS */
void DemuxReset(void) {}
void DemuxAbort(void) {}
void DemuxFlush(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
void FillBuffer(bool mode) {}

/* Various helper functions */
bool CanPauseStream() { return false; }
bool CanSeekStream() { return false; }

/* Callbacks */
void PauseStream(bool bPaused) {}
bool SeekTime(double time, bool backwards, double *startpts) { return false; }
void SetSpeed(int speed) {}
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }

const char* GetBackendHostname()
{
	return octonetAddress.c_str();
}

}
