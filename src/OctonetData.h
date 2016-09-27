#pragma once

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

#include <vector>

#include "p8-platform/threads/threads.h"
#include "p8-platform/util/StdString.h"
#include "client.h"

struct OctonetEpgEntry
{
	int64_t channelId;
	time_t start;
	time_t end;
	int id;
	std::string title;
	std::string subtitle;
};

struct OctonetChannel
{
	int64_t nativeId;
	std::string name;
	std::string url;
	bool radio;
	int id;

	std::vector<OctonetEpgEntry> epg;
};

struct OctonetGroup
{
	std::string name;
	bool radio;
	std::vector<int> members;
};

class OctonetData : public P8PLATFORM::CThread
{
	public:
		OctonetData(void);
		virtual ~OctonetData(void);

		virtual int getChannelCount(void);
		virtual PVR_ERROR getChannels(ADDON_HANDLE handle, bool bRadio);

		virtual int getGroupCount(void);
		virtual PVR_ERROR getGroups(ADDON_HANDLE handle, bool bRadio);
		virtual PVR_ERROR getGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);

		virtual PVR_ERROR getEPG(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t start, time_t end);

	protected:
		virtual bool loadChannelList(void);
		virtual bool loadEPG(void);
		virtual OctonetGroup* findGroup(const std::string &name);

		virtual void *Process(void);

		OctonetChannel* findChannel(int64_t nativeId);
		time_t parseDateTime(std::string date);
		int64_t parseID(std::string id);

	private:
		std::string serverAddress;
		std::vector<OctonetChannel> channels;
		std::vector<OctonetGroup> groups;

		time_t lastEpgLoad;
};
