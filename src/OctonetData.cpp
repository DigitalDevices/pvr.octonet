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

#include <json/json.h>
#include <sstream>
#include <string>

#ifdef __WINDOWS__
#define timegm _mkgmtime
#endif

using namespace ADDON;

OctonetData::OctonetData()
{
  m_serverAddress = octonetAddress;
  m_channels.clear();
  m_groups.clear();
  m_lastEpgLoad = 0;

  if (!LoadChannelList())
    libKodi->QueueNotification(QUEUE_ERROR, libKodi->GetLocalizedString(30001), m_channels.size());
}

OctonetData::~OctonetData(void)
{
  m_channels.clear();
  m_groups.clear();
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
  void* f =
      libKodi->OpenFile(("http://" + m_serverAddress + "/channellist.lua?select=json").c_str(), 0);
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
  std::vector<OctonetChannel>::iterator it;
  for (it = m_channels.begin(); it < m_channels.end(); ++it)
  {
    if (it->nativeId == nativeId)
      return &*it;
  }

  return NULL;
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
  if (m_lastEpgLoad + 30 > time(NULL))
    return false;

  std::string jsonContent;
  void* f = libKodi->OpenFile(("http://" + m_serverAddress + "/epg.lua?;#|encoding=gzip").c_str(), 0);
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
  OctonetChannel* channel = NULL;
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
    entry.id = atoi(epgId.c_str());

    if (channel == NULL || channel->nativeId != entry.channelId)
      channel = FindChannel(entry.channelId);

    if (channel == NULL)
    {
      libKodi->Log(LOG_ERROR, "EPG for unknown channel.");
      continue;
    }

    channel->epg.push_back(entry);
  }

  m_lastEpgLoad = time(NULL);
  return true;
}

void* OctonetData::Process(void)
{
  return NULL;
}

int OctonetData::getChannelCount(void)
{
  return m_channels.size();
}

PVR_ERROR OctonetData::getChannels(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int i = 0; i < m_channels.size(); i++)
  {
    OctonetChannel& channel = m_channels.at(i);
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
  for (unsigned int i = 0; i < m_channels.size(); i++)
  {
    OctonetChannel& chan = m_channels.at(i);
    if (iChannelUid != chan.id)
      continue;

    if (chan.epg.empty())
    {
      LoadEPG();
    }

    // FIXME: Check if reload is needed!?

    std::vector<OctonetEpgEntry>::iterator it;
    time_t last_end = 0;
    for (it = chan.epg.begin(); it != chan.epg.end(); ++it)
    {
      if (it->end > last_end)
        last_end = it->end;

      if (it->end < start || it->start > end)
      {
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
      LoadEPG();

    for (it = chan.epg.begin(); it != chan.epg.end(); ++it)
    {
      if (it->end < start || it->start > end)
      {
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

const std::string& OctonetData::GetUrl(int id) const
{
  for (std::vector<OctonetChannel>::const_iterator iter = m_channels.begin(); iter != m_channels.end();
       ++iter)
  {
    if (iter->id == id)
    {
      return iter->url;
    }
  }

  return m_channels[0].url;
}

const std::string& OctonetData::GetName(int id) const
{
  for (std::vector<OctonetChannel>::const_iterator iter = m_channels.begin(); iter != m_channels.end();
       ++iter)
  {
    if (iter->id == id)
    {
      return iter->name;
    }
  }

  return m_channels[0].name;
}

int OctonetData::getGroupCount(void)
{
  return m_groups.size();
}

PVR_ERROR OctonetData::getGroups(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int i = 0; i < m_groups.size(); i++)
  {
    OctonetGroup& group = m_groups.at(i);
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

PVR_ERROR OctonetData::getGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP& group)
{
  OctonetGroup* g = FindGroup(group.strGroupName);
  if (g == NULL)
    return PVR_ERROR_UNKNOWN;

  for (unsigned int i = 0; i < g->members.size(); i++)
  {
    OctonetChannel& channel = m_channels.at(g->members[i]);
    PVR_CHANNEL_GROUP_MEMBER m;
    memset(&m, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

    strncpy(m.strGroupName, group.strGroupName, strlen(group.strGroupName));
    m.iChannelUniqueId = channel.id;
    m.iChannelNumber = channel.id;

    pvr->TransferChannelGroupMember(handle, &m);
  }

  return PVR_ERROR_NO_ERROR;
}

OctonetGroup* OctonetData::FindGroup(const std::string& name)
{
  for (unsigned int i = 0; i < m_groups.size(); i++)
  {
    if (m_groups.at(i).name == name)
      return &m_groups.at(i);
  }

  return NULL;
}
