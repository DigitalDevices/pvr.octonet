/*
 *  Copyright (C) 2015 Julian Scheel <julian@jusst.de>
 *  Copyright (C) 2015 jusst technologies GmbH
 *  Copyright (C) 2015 Digital Devices GmbH
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 *
 */

#include "addon.h"

#include "OctonetData.h"

ADDON_STATUS COctonetAddon::SetSetting(const std::string& settingName,
                                       const kodi::CSettingValue& settingValue)
{
  /* For simplicity do a full addon restart whenever settings are
   * changed */
  return ADDON_STATUS_NEED_RESTART;
}

ADDON_STATUS COctonetAddon::CreateInstance(int instanceType,
                                           const std::string& instanceID,
                                           KODI_HANDLE instance,
                                           const std::string& version,
                                           KODI_HANDLE& addonInstance)
{
  if (instanceType == ADDON_INSTANCE_PVR)
  {
    kodi::Log(ADDON_LOG_DEBUG, "%s: Creating octonet pvr instance", __func__);

    /* IP or hostname of the octonet to be connected to */
    std::string octonetAddress = kodi::GetSettingString("octonetAddress");

    OctonetData* usedInstance = new OctonetData(octonetAddress, instance, version);
    addonInstance = usedInstance;

    m_usedInstances.emplace(instanceID, usedInstance);
    return ADDON_STATUS_OK;
  }

  return ADDON_STATUS_UNKNOWN;
}

void COctonetAddon::DestroyInstance(int instanceType,
                                    const std::string& instanceID,
                                    KODI_HANDLE addonInstance)
{
  if (instanceType == ADDON_INSTANCE_PVR)
  {
    kodi::Log(ADDON_LOG_DEBUG, "%s: Destoying octonet pvr instance", __func__);

    const auto& it = m_usedInstances.find(instanceID);
    if (it != m_usedInstances.end())
    {
      m_usedInstances.erase(it);
    }
  }
}

ADDONCREATOR(COctonetAddon)
