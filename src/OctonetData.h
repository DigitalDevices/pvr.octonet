/*
 *  Copyright (C) 2015 Julian Scheel <julian@jusst.de>
 *  Copyright (C) 2015 jusst technologies GmbH
 *  Copyright (C) 2015 Digital Devices GmbH
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 *
 */

#pragma once

#include <vector>

#include "p8-platform/threads/threads.h"
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

		virtual PVR_ERROR getEPG(ADDON_HANDLE handle, int iChannelUid, time_t start, time_t end);
		const std::string& getUrl(int id) const;
		const std::string& getName(int id) const;

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
