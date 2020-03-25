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

#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"

#ifndef __func__
#define __func__ __FUNCTION__
#endif

extern ADDON::CHelper_libXBMC_addon *libKodi;
extern CHelper_libXBMC_pvr *pvr;

/* IP or hostname of the octonet to be connected to */
extern std::string octonetAddress;
