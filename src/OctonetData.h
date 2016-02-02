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

#include "platform/threads/threads.h"
#include "platform/util/StdString.h"
#include "client.h"

struct OctonetChannel
{
	std::string name;
	std::string url;
	bool radio;
	int id;
};

class OctonetData : public PLATFORM::CThread
{
	public:
		OctonetData(void);
		virtual ~OctonetData(void);

		virtual int getChannelCount(void);
		virtual PVR_ERROR getChannels(ADDON_HANDLE handle, bool bRadio);

	protected:
		virtual bool loadChannelList(void);

		virtual void *Process(void);

	private:
		std::string serverAddress;
		std::vector<OctonetChannel> channels;
};
