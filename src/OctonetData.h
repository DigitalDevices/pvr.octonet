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

#include <atomic>
#include <kodi/addon-instance/PVR.h>
#include <thread>
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

class ATTR_DLL_LOCAL OctonetData : public kodi::addon::CInstancePVRClient
{
public:
  OctonetData(const std::string& octonetAddress,
              const kodi::addon::IInstanceInfo& instance);
  ~OctonetData() override;

  PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;
  PVR_ERROR GetBackendName(std::string& name) override;
  PVR_ERROR GetBackendVersion(std::string& version) override;
  PVR_ERROR GetConnectionString(std::string& connection) override;
  PVR_ERROR GetBackendHostname(std::string& hostname) override;

  PVR_ERROR OnSystemSleep() override;
  PVR_ERROR OnSystemWake() override;

  PVR_ERROR GetChannelsAmount(int& amount) override;
  PVR_ERROR GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results) override;

  PVR_ERROR GetChannelGroupsAmount(int& amount) override;
  PVR_ERROR GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results) override;
  PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                   kodi::addon::PVRChannelGroupMembersResultSet& results) override;

  PVR_ERROR GetEPGForChannel(int channelUid,
                             time_t start,
                             time_t end,
                             kodi::addon::PVREPGTagsResultSet& results) override;

  bool OpenLiveStream(const kodi::addon::PVRChannel& channelinfo) override;
  int ReadLiveStream(unsigned char* buffer, unsigned int size) override;
  void CloseLiveStream() override;

protected:
  void Process();

  const std::string& GetUrl(int id) const;
  const std::string& GetName(int id) const;

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

  std::atomic<bool> m_running = {false};
  std::atomic<bool> m_readInitialEPG = {false};
  std::thread m_thread;
};
