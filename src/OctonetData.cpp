/*
 *  Copyright (C) 2015 Julian Scheel <julian@jusst.de>
 *  Copyright (C) 2015 jusst technologies GmbH
 *  Copyright (C) 2015 Digital Devices GmbH
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 *
 */

#include "OctonetData.h"

#include "rtsp_client.hpp"

#include <json/json.h>
#include <kodi/Filesystem.h>
#include <kodi/General.h>
#include <sstream>
#include <string>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define timegm _mkgmtime
#endif

OctonetData::OctonetData(const std::string& octonetAddress,
                         KODI_HANDLE instance,
                         const std::string& kodiVersion)
  : kodi::addon::CInstancePVRClient(instance, kodiVersion)
{
  m_serverAddress = octonetAddress;
  m_channels.clear();
  m_groups.clear();
  m_lastEpgLoad = 0;

  if (!LoadChannelList())
    kodi::QueueFormattedNotification(QUEUE_ERROR, kodi::GetLocalizedString(30001).c_str(),
                                     m_channels.size());

  /*
  // Currently unused, as thread was already present before with
  // p8platform, by remove of them was it added as C++11 thread way.
  kodi::Log(ADDON_LOG_INFO, "%s Starting separate client update thread...", __func__);
  m_running = true;
  m_thread = std::thread([&] { Process(); });
  */
}

OctonetData::~OctonetData(void)
{
  /*
  m_running = false;
  if (m_thread.joinable())
    m_thread.join();
  */
}

PVR_ERROR OctonetData::GetCapabilities(kodi::addon::PVRCapabilities& capabilities)
{
  capabilities.SetSupportsTV(true);
  capabilities.SetSupportsRadio(true);
  capabilities.SetSupportsChannelGroups(true);
  capabilities.SetSupportsEPG(true);
  capabilities.SetSupportsRecordings(false);
  capabilities.SetSupportsRecordingsRename(false);
  capabilities.SetSupportsRecordingsLifetimeChange(false);
  capabilities.SetSupportsDescrambleInfo(false);

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetBackendName(std::string& name)
{
  name = "Digital Devices Octopus NET Client";
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetBackendVersion(std::string& version)
{
  version = STR(OCTONET_VERSION);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetConnectionString(std::string& connection)
{
  connection = "connected"; // FIXME: translate?
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetBackendHostname(std::string& hostname)
{
  hostname = m_serverAddress;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::OnSystemSleep()
{
  kodi::Log(ADDON_LOG_INFO, "Received event: %s", __func__);
  // FIXME: Disconnect?
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::OnSystemWake()
{
  kodi::Log(ADDON_LOG_INFO, "Received event: %s", __func__);
  // FIXME:Reconnect?
  return PVR_ERROR_NO_ERROR;
}

int64_t OctonetData::ParseID(std::string id)
{
  std::hash<std::string> hash_fn;
  int64_t nativeId = hash_fn(id);

  return nativeId;
}

bool OctonetData::LoadChannelList()
{
  std::string jsonContent;
  kodi::vfs::CFile f;
  if (!f.OpenFile("http://" + m_serverAddress + "/channellist.lua?select=json", 0))
    return false;

  char buf[1024];
  while (int read = f.Read(buf, 1024))
    jsonContent.append(buf, read);

  f.Close();

  Json::Value root;
  Json::Reader reader;

  if (!reader.parse(jsonContent, root, false))
    return false;

  const Json::Value groupList = root["GroupList"];
  for (unsigned int i = 0; i < groupList.size(); i++)
  {
    const Json::Value channelList = groupList[i]["ChannelList"];
    OctonetGroup group;

    group.name = groupList[i]["Title"].asString();
    group.radio = group.name.compare(0, 5, "Radio") ? false : true;

    for (unsigned int j = 0; j < channelList.size(); j++)
    {
      const Json::Value channel = channelList[j];
      OctonetChannel chan;

      chan.name = channel["Title"].asString();
      chan.url = "rtsp://" + m_serverAddress + "/" + channel["Request"].asString();
      chan.radio = group.radio;
      chan.nativeId = ParseID(channel["ID"].asString());

      chan.id = 1000 + m_channels.size();
      group.members.push_back(m_channels.size());
      m_channels.push_back(chan);
    }
    m_groups.push_back(group);
  }

  return true;
}

OctonetChannel* OctonetData::FindChannel(int64_t nativeId)
{
  for (auto& channel : m_channels)
  {
    if (channel.nativeId == nativeId)
      return &channel;
  }

  return nullptr;
}

time_t OctonetData::ParseDateTime(std::string date)
{
  struct tm timeinfo;

  memset(&timeinfo, 0, sizeof(timeinfo));

  if (date.length() > 8)
  {
    sscanf(date.c_str(), "%04d-%02d-%02dT%02d:%02d:%02dZ", &timeinfo.tm_year, &timeinfo.tm_mon,
           &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
    timeinfo.tm_mon -= 1;
    timeinfo.tm_year -= 1900;
  }
  else
  {
    sscanf(date.c_str(), "%02d:%02d:%02d", &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
    timeinfo.tm_year = 70; // unix timestamps start 1970
    timeinfo.tm_mday = 1;
  }

  timeinfo.tm_isdst = -1;

  return timegm(&timeinfo);
}

bool OctonetData::LoadEPG(void)
{
  /* Reload at most every 30 seconds */
  if (m_lastEpgLoad + 30 > time(nullptr))
    return false;

  std::string jsonContent;
  kodi::vfs::CFile f;
  if (!f.OpenFile("http://" + m_serverAddress + "/epg.lua?;#|encoding=gzip", 0))
    return false;

  char buf[1024];
  while (int read = f.Read(buf, 1024))
    jsonContent.append(buf, read);

  f.Close();

  Json::Value root;
  Json::Reader reader;

  if (!reader.parse(jsonContent, root, false))
    return false;

  const Json::Value eventList = root["EventList"];
  OctonetChannel* channel = nullptr;
  for (unsigned int i = 0; i < eventList.size(); i++)
  {
    const Json::Value event = eventList[i];
    OctonetEpgEntry entry;

    entry.start = ParseDateTime(event["Time"].asString());
    entry.end = entry.start + ParseDateTime(event["Duration"].asString());
    entry.title = event["Name"].asString();
    entry.subtitle = event["Text"].asString();
    std::string channelId = event["ID"].asString();
    std::string epgId = channelId.substr(channelId.rfind(":") + 1);
    channelId = channelId.substr(0, channelId.rfind(":"));

    entry.channelId = ParseID(channelId);
    entry.id = std::stoi(epgId);

    if (channel == nullptr || channel->nativeId != entry.channelId)
      channel = FindChannel(entry.channelId);

    if (channel == nullptr)
    {
      kodi::Log(ADDON_LOG_ERROR, "EPG for unknown channel.");
      continue;
    }

    channel->epg.push_back(entry);
  }

  m_lastEpgLoad = time(nullptr);
  return true;
}

void OctonetData::Process()
{
  return;
}

PVR_ERROR OctonetData::GetChannelsAmount(int& amount)
{
  amount = m_channels.size();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results)
{
  for (unsigned int i = 0; i < m_channels.size(); i++)
  {
    OctonetChannel& channel = m_channels.at(i);
    if (channel.radio == radio)
    {
      kodi::addon::PVRChannel chan;

      chan.SetUniqueId(channel.id);
      chan.SetIsRadio(channel.radio);
      chan.SetChannelNumber(i);
      chan.SetChannelName(channel.name);
      chan.SetMimeType("video/x-mpegts");
      chan.SetIsHidden(false);

      results.Add(chan);
    }
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetEPGForChannel(int channelUid,
                                        time_t start,
                                        time_t end,
                                        kodi::addon::PVREPGTagsResultSet& results)
{
  for (unsigned int i = 0; i < m_channels.size(); i++)
  {
    OctonetChannel& chan = m_channels.at(i);
    if (channelUid != chan.id)
      continue;

    if (chan.epg.empty())
    {
      LoadEPG();
    }

    // FIXME: Check if reload is needed!?

    time_t last_end = 0;
    for (const auto& epg : chan.epg)
    {
      if (epg.end > last_end)
        last_end = epg.end;

      if (epg.end < start || epg.start > end)
      {
        continue;
      }

      kodi::addon::PVREPGTag entry;

      entry.SetUniqueChannelId(chan.id);
      entry.SetUniqueBroadcastId(epg.id);
      entry.SetTitle(epg.title);
      entry.SetPlotOutline(epg.subtitle);
      entry.SetStartTime(epg.start);
      entry.SetEndTime(epg.end);

      results.Add(entry);
    }

    if (last_end < end)
      LoadEPG();

    for (const auto& epg : chan.epg)
    {
      if (epg.end < start || epg.start > end)
      {
        continue;
      }

      kodi::addon::PVREPGTag entry;

      entry.SetUniqueChannelId(chan.id);
      entry.SetUniqueBroadcastId(epg.id);
      entry.SetTitle(epg.title);
      entry.SetPlotOutline(epg.subtitle);
      entry.SetStartTime(epg.start);
      entry.SetEndTime(epg.end);

      results.Add(entry);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

const std::string& OctonetData::GetUrl(int id) const
{
  for (const auto& channel : m_channels)
  {
    if (channel.id == id)
    {
      return channel.url;
    }
  }

  return m_channels[0].url;
}

const std::string& OctonetData::GetName(int id) const
{
  for (const auto& channel : m_channels)
  {
    if (channel.id == id)
    {
      return channel.name;
    }
  }

  return m_channels[0].name;
}

PVR_ERROR OctonetData::GetChannelGroupsAmount(int& amount)
{
  amount = m_groups.size();
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results)
{
  for (const auto& group : m_groups)
  {
    if (group.radio == radio)
    {
      kodi::addon::PVRChannelGroup g;

      g.SetPosition(0);
      g.SetIsRadio(group.radio);
      g.SetGroupName(group.name);

      results.Add(g);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                              kodi::addon::PVRChannelGroupMembersResultSet& results)
{
  const OctonetGroup* g = FindGroup(group.GetGroupName());
  if (g == nullptr)
    return PVR_ERROR_UNKNOWN;

  for (unsigned int i = 0; i < g->members.size(); i++)
  {
    OctonetChannel& channel = m_channels.at(g->members[i]);
    kodi::addon::PVRChannelGroupMember m;

    m.SetGroupName(group.GetGroupName());
    m.SetChannelUniqueId(channel.id);
    m.SetChannelNumber(channel.id);

    results.Add(m);
  }

  return PVR_ERROR_NO_ERROR;
}

OctonetGroup* OctonetData::FindGroup(const std::string& name)
{
  for (auto& group : m_groups)
  {
    if (group.name == name)
      return &group;
  }

  return nullptr;
}

/* PVR stream handling */
/* entirely unused, as we use standard RTSP+TS mux, which can be handlded by
 * Kodi core */
bool OctonetData::OpenLiveStream(const kodi::addon::PVRChannel& channelinfo)
{
  return rtsp_open(GetName(channelinfo.GetUniqueId()), GetUrl(channelinfo.GetUniqueId()));
}

int OctonetData::ReadLiveStream(unsigned char* pBuffer, unsigned int iBufferSize)
{
  return rtsp_read(pBuffer, iBufferSize);
}

void OctonetData::CloseLiveStream()
{
  rtsp_close();
}
