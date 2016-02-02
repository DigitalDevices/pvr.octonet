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
#include "platform/util/StringUtils.h"

using namespace ADDON;

OctonetData::OctonetData()
{
	serverAddress = octonetAddress;
	channels.clear();

	if (loadChannelList())
		kodi->QueueNotification(QUEUE_INFO, "%d channels loaded.", channels.size());
}

OctonetData::~OctonetData(void)
{
	channels.clear();
}

bool OctonetData::loadChannelList()
{
	std::string jsonContent;
	void *f = kodi->OpenFile(("http://" + serverAddress + "/channellist.lua?select=json").c_str(), 0);
	if (!f)
		return false;

	char buf[1024];
	while (int read = kodi->ReadFile(f, buf, 1024))
		jsonContent.append(buf, read);

	kodi->CloseFile(f);

	Json::Value root;
	Json::Reader reader;

	if (!reader.parse(jsonContent, root, false))
		return false;

	const Json::Value groupList = root["GroupList"];
	for (unsigned int i = 0; i < groupList.size(); i++) {
		const Json::Value channelList = groupList[i]["ChannelList"];
		for (unsigned int j = 0; j < channelList.size(); j++) {
			const Json::Value channel = channelList[j];
			OctonetChannel chan;

			chan.name = channel["Title"].asString();
			chan.url = "rtsp://" + serverAddress + "/" + channel["Request"].asString();
			chan.radio = false;

#if 0 /* Would require a 64 bit identifier */
			std::string id = channel["ID"].asString();
			size_t strip;
			/* Strip colons from id */
			while ((strip = id.find(":")) != std::string::npos)
				id.erase(strip, 1);

			std::stringstream ids(id);
			ids >> chan.id;
#endif
			chan.id = 1000 + channels.size();
			channels.push_back(chan);
		}
	}

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
			strncpy(chan.strStreamURL, channel.url.c_str(), strlen(channel.url.c_str()));
			strcpy(chan.strInputFormat, "video/x-mpegts");
			chan.bIsHidden = false;

			pvr->TransferChannelEntry(handle, &chan);
		}
	}
	return PVR_ERROR_NO_ERROR;
}
