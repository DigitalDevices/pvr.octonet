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

#include "kodi/libXBMC_addon.h"
#include "kodi/libXBMC_pvr.h"

#ifndef __func__
#define __func__ __FUNCTION__
#endif

extern ADDON::CHelper_libXBMC_addon *kodi;
extern CHelper_libXBMC_pvr *pvr;

/* IP or hostname of the octonet to be connected to */
extern std::string octonetAddress;
