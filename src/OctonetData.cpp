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

#include <sstream>
#include <string>

#include <json/json.h>

#include "OctonetData.h"

#ifdef __WINDOWS__
#define timegm _mkgmtime
#endif

using namespace ADDON;

OctonetData::OctonetData()
{
	serverAddress = octonetAddress;
	channels.clear();
	groups.clear();
	lastEpgLoad = 0;

	if (!loadChannelList())
		libKodi->QueueNotification(QUEUE_ERROR, libKodi->GetLocalizedString(30001), channels.size());
}

OctonetData::~OctonetData(void)
{
	channels.clear();
	groups.clear();
}

int64_t OctonetData::parseID(std::string id)
{
	std::hash<std::string> hash_fn;
	int64_t nativeId = hash_fn(id);

	return nativeId;
}

bool OctonetData::loadChannelList()
{
	std::string jsonContent;
	void *f = libKodi->OpenFile(("http://" + serverAddress + "/channellist.lua?select=json").c_str(), 0);
	if (!f)
		return false;

	char buf[1024];
	while (int read = libKodi->ReadFile(f, buf, 1024))
		jsonContent.append(buf, read);

	libKodi->CloseFile(f);

	Json::Value root;
	Json::Reader reader;

	if (!reader.parse(jsonContent, root, false))
		return false;

	const Json::Value groupList = root["GroupList"];
	for (unsigned int i = 0; i < groupList.size(); i++) {
		const Json::Value channelList = groupList[i]["ChannelList"];
		OctonetGroup group;

		group.name = groupList[i]["Title"].asString();
		group.radio = group.name.compare(0, 5, "Radio") ? false : true;

		for (unsigned int j = 0; j < channelList.size(); j++) {
			const Json::Value channel = channelList[j];
			OctonetChannel chan;

			chan.name = channel["Title"].asString();
			chan.url = "rtsp://" + serverAddress + "/" + channel["Request"].asString();
			chan.radio = group.radio;
			chan.nativeId = parseID(channel["ID"].asString());

			chan.id = 1000 + channels.size();
			group.members.push_back(channels.size());
			channels.push_back(chan);
		}
		groups.push_back(group);
	}

	return true;
}

OctonetChannel* OctonetData::findChannel(int64_t nativeId)
{
	std::vector<OctonetChannel>::iterator it;
	for (it = channels.begin(); it < channels.end(); ++it) {
		if (it->nativeId == nativeId)
			return &*it;
	}

	return NULL;
}

time_t OctonetData::parseDateTime(std::string date)
{
	struct tm timeinfo;

	memset(&timeinfo, 0, sizeof(timeinfo));

	if (date.length() > 8) {
		sscanf(date.c_str(), "%04d-%02d-%02dT%02d:%02d:%02dZ",
				&timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday,
				&timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
		timeinfo.tm_mon -= 1;
		timeinfo.tm_year -= 1900;
	} else {
		sscanf(date.c_str(), "%02d:%02d:%02d",
				&timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
		timeinfo.tm_year = 70; // unix timestamps start 1970
		timeinfo.tm_mday = 1;
	}

	timeinfo.tm_isdst = -1;

	return timegm(&timeinfo);
}

bool OctonetData::loadEPG(void)
{
	/* Reload at most every 30 seconds */
	if (lastEpgLoad + 30 > time(NULL))
		return false;

	std::string jsonContent;
	void *f = libKodi->OpenFile(("http://" + serverAddress + "/epg.lua?;#|encoding=gzip").c_str(), 0);
	if (!f)
		return false;

	char buf[1024];
	while (int read = libKodi->ReadFile(f, buf, 1024))
		jsonContent.append(buf, read);

	libKodi->CloseFile(f);

	Json::Value root;
	Json::Reader reader;

	if (!reader.parse(jsonContent, root, false))
		return false;

	const Json::Value eventList = root["EventList"];
	OctonetChannel *channel = NULL;
	for (unsigned int i = 0; i < eventList.size(); i++) {
		const Json::Value event = eventList[i];
		OctonetEpgEntry entry;

		entry.start = parseDateTime(event["Time"].asString());
		entry.end = entry.start + parseDateTime(event["Duration"].asString());
		entry.title = event["Name"].asString();
		entry.subtitle = event["Text"].asString();
		std::string channelId = event["ID"].asString();
		std::string epgId = channelId.substr(channelId.rfind(":") + 1);
		channelId = channelId.substr(0, channelId.rfind(":"));

		entry.channelId = parseID(channelId);
		entry.id = atoi(epgId.c_str());

		if (channel == NULL || channel->nativeId != entry.channelId)
			channel = findChannel(entry.channelId);

		if (channel == NULL) {
			libKodi->Log(LOG_ERROR, "EPG for unknown channel.");
			continue;
		}

		channel->epg.push_back(entry);
	}

	lastEpgLoad = time(NULL);
	return true;
}

void *OctonetData::Process(void)
{
	return NULL;
}

int OctonetData::getChannelCount(void)
{
	return channels.size();
}

PVR_ERROR OctonetData::getChannels(ADDON_HANDLE handle, bool bRadio)
{
	for (unsigned int i = 0; i < channels.size(); i++)
	{
		OctonetChannel &channel = channels.at(i);
		if (channel.radio == bRadio)
		{
			PVR_CHANNEL chan;
			memset(&chan, 0, sizeof(PVR_CHANNEL));

			chan.iUniqueId = channel.id;
			chan.bIsRadio = channel.radio;
			chan.iChannelNumber = i;
			strncpy(chan.strChannelName, channel.name.c_str(), strlen(channel.name.c_str()));
			strcpy(chan.strInputFormat, "video/x-mpegts");
			chan.bIsHidden = false;

			pvr->TransferChannelEntry(handle, &chan);
		}
	}
	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::getEPG(ADDON_HANDLE handle, int iChannelUid, time_t start, time_t end)
{
	for (unsigned int i = 0; i < channels.size(); i++)
	{
		OctonetChannel &chan = channels.at(i);
		if (iChannelUid != chan.id)
			continue;

		if(chan.epg.empty()) {
			loadEPG();
		}

		// FIXME: Check if reload is needed!?

		std::vector<OctonetEpgEntry>::iterator it;
		time_t last_end = 0;
		for (it = chan.epg.begin(); it != chan.epg.end(); ++it) {
			if (it->end > last_end)
				last_end = it->end;

			if (it->end < start || it->start > end) {
				continue;
			}

			EPG_TAG entry;
			memset(&entry, 0, sizeof(EPG_TAG));
			entry.iSeriesNumber = EPG_TAG_INVALID_SERIES_EPISODE;
			entry.iEpisodeNumber = EPG_TAG_INVALID_SERIES_EPISODE;
			entry.iEpisodePartNumber = EPG_TAG_INVALID_SERIES_EPISODE;

			entry.iUniqueChannelId = chan.id;
			entry.iUniqueBroadcastId = it->id;
			entry.strTitle = it->title.c_str();
			entry.strPlotOutline = it->subtitle.c_str();
			entry.startTime = it->start;
			entry.endTime = it->end;

			pvr->TransferEpgEntry(handle, &entry);
		}

		if (last_end < end)
			loadEPG();

		for (it = chan.epg.begin(); it != chan.epg.end(); ++it) {
			if (it->end < start || it->start > end) {
				continue;
			}

			EPG_TAG entry;
			memset(&entry, 0, sizeof(EPG_TAG));
			entry.iSeriesNumber = EPG_TAG_INVALID_SERIES_EPISODE;
			entry.iEpisodeNumber = EPG_TAG_INVALID_SERIES_EPISODE;
			entry.iEpisodePartNumber = EPG_TAG_INVALID_SERIES_EPISODE;

			entry.iUniqueChannelId = chan.id;
			entry.iUniqueBroadcastId = it->id;
			entry.strTitle = it->title.c_str();
			entry.strPlotOutline = it->subtitle.c_str();
			entry.startTime = it->start;
			entry.endTime = it->end;

			pvr->TransferEpgEntry(handle, &entry);
		}
	}

	return PVR_ERROR_NO_ERROR;
}

const std::string& OctonetData::getUrl(int id) const {
	for(std::vector<OctonetChannel>::const_iterator iter = channels.begin(); iter != channels.end(); ++iter) {
		if(iter->id == id) {
			return iter->url;
		}
	}

	return channels[0].url;
}

const std::string& OctonetData::getName(int id) const {
	for(std::vector<OctonetChannel>::const_iterator iter = channels.begin(); iter != channels.end(); ++iter) {
		if(iter->id == id) {
			return iter->name;
		}
	}

	return channels[0].name;
}

int OctonetData::getGroupCount(void)
{
	return groups.size();
}

PVR_ERROR OctonetData::getGroups(ADDON_HANDLE handle, bool bRadio)
{
	for (unsigned int i = 0; i < groups.size(); i++)
	{
		OctonetGroup &group = groups.at(i);
		if (group.radio == bRadio)
		{
			PVR_CHANNEL_GROUP g;
			memset(&g, 0, sizeof(PVR_CHANNEL_GROUP));

			g.iPosition = 0;
			g.bIsRadio = group.radio;
			strncpy(g.strGroupName, group.name.c_str(), strlen(group.name.c_str()));

			pvr->TransferChannelGroup(handle, &g);
		}
	}

	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::getGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
	OctonetGroup *g = findGroup(group.strGroupName);
	if (g == NULL)
		return PVR_ERROR_UNKNOWN;

	for (unsigned int i = 0; i < g->members.size(); i++)
	{
		OctonetChannel &channel = channels.at(g->members[i]);
		PVR_CHANNEL_GROUP_MEMBER m;
		memset(&m, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

		strncpy(m.strGroupName, group.strGroupName, strlen(group.strGroupName));
		m.iChannelUniqueId = channel.id;
		m.iChannelNumber = channel.id;

		pvr->TransferChannelGroupMember(handle, &m);
	}

	return PVR_ERROR_NO_ERROR;
}

OctonetGroup* OctonetData::findGroup(const std::string &name)
{
	for (unsigned int i = 0; i < groups.size(); i++)
	{
		if (groups.at(i).name == name)
			return &groups.at(i);
	}

	return NULL;
}
