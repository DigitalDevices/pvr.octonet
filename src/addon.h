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

#include <kodi/AddonBase.h>
#include <unordered_map>

class OctonetData;

class ATTRIBUTE_HIDDEN COctonetAddon : public kodi::addon::CAddonBase
{
public:
  COctonetAddon() = default;

  ADDON_STATUS SetSetting(const std::string& settingName,
                          const kodi::CSettingValue& settingValue) override;
  ADDON_STATUS CreateInstance(int instanceType,
                              const std::string& instanceID,
                              KODI_HANDLE instance,
                              const std::string& version,
                              KODI_HANDLE& addonInstance) override;
  void DestroyInstance(int instanceType,
                       const std::string& instanceID,
                       KODI_HANDLE addonInstance) override;

private:
  std::unordered_map<std::string, OctonetData*> m_usedInstances;
};
