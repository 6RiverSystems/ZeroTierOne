/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_OSXETHERNETTAP_HPP
#define ZT_OSXETHERNETTAP_HPP

#include <stdio.h>
#include <stdlib.h>

#include <stdexcept>

#include "Thread.hpp"

namespace ZeroTier {

/**
 * OSX Ethernet tap using ZeroTier kernel extension zt# devices
 */
class OSXEthernetTap
{
public:
	OSXEthernetTap(
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *desiredDevice,
		const char *friendlyName,
		void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
		void *arg);

	~OSXEthernetTap();

	void setEnabled(bool en);
	bool enabled() const;
	bool addIP(const InetAddress &ip);
	bool removeIP(const InetAddress &ip);
	std::set<InetAddress> ips() const;
	void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	std::string deviceName() const;
	void setFriendlyName(const char *friendlyName);
	bool updateMulticastGroups(std::set<MulticastGroup> &groups);

	void threadMain()
		throw();

private:
	void (*_handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &);
	void *_arg;
	Thread _thread;
	std::string _dev;
	unsigned int _mtu;
	unsigned int _metric;
	int _fd;
	int _shutdownSignalPipe[2];
	volatile bool _enabled;
};

} // namespace ZeroTier

#endif
