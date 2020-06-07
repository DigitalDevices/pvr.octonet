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

#include "client.h"

#include "p8-platform/threads/threads.h"

#include <vector>

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
  virtual PVR_ERROR getGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP& group);

  virtual PVR_ERROR getEPG(ADDON_HANDLE handle, int iChannelUid, time_t start, time_t end);
  const std::string& GetUrl(int id) const;
  const std::string& GetName(int id) const;

protected:
  void* Process(void) override;

  bool LoadChannelList(void);
  bool LoadEPG(void);
  OctonetGroup* FindGroup(const std::string& name);
  OctonetChannel* FindChannel(int64_t nativeId);
  time_t ParseDateTime(std::string date);
  int64_t ParseID(std::string id);

private:
  std::string m_serverAddress;
  std::vector<OctonetChannel> m_channels;
  std::vector<OctonetGroup> m_groups;

  time_t m_lastEpgLoad;
};
