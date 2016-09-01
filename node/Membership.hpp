/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_MEMBERSHIP_HPP
#define ZT_MEMBERSHIP_HPP

#include <stdint.h>

#include <map>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "CertificateOfMembership.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Hashtable.hpp"
#include "NetworkConfig.hpp"

// Expiration time for capability and tag cache
#define ZT_MEMBERSHIP_STATE_EXPIRATION_TIME 600000

// Expiration time for Memberships (used in Peer::clean())
#define ZT_MEMBERSHIP_EXPIRATION_TIME (ZT_MEMBERSHIP_STATE_EXPIRATION_TIME * 2)

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * A container for certificates of membership and other network credentials
 *
 * This is kind of analogous to a join table between Peer and Network. It is
 * presently held by the Network object for each participating Peer.
 *
 * This is not thread safe. It must be locked externally.
 */
class Membership
{
private:
	// Tags and related state
	struct TState
	{
		TState() : lastPushed(0),lastReceived(0) {}
		// Last time we pushed OUR tag to this peer (with this ID)
		uint64_t lastPushed;
		// Last time we received THEIR tag (with this ID)
		uint64_t lastReceived;
		// THEIR tag
		Tag tag;
	};

	// Credentials and related state
	struct CState
	{
		CState() : lastPushed(0),lastReceived(0) {}
		// Last time we pushed OUR capability to this peer (with this ID)
		uint64_t lastPushed;
		// Last time we received THEIR capability (with this ID)
		uint64_t lastReceived;
		// THEIR capability
		Capability cap;
	};

public:
	/**
	 * A wrapper to iterate through member capabilities in ascending order of capability ID and return only valid ones
	 */
	class CapabilityIterator
	{
	public:
		CapabilityIterator(const Membership &m) :
			_m(m),
			_i(m._caps.begin()),
			_e(m._caps.end())
		{
		}

		inline const Capability *next(const NetworkConfig &nconf)
		{
			while (_i != _e) {
				if ((_i->second.lastReceived)&&(_m.isCredentialTimestampValid(nconf,_i->second.cap)))
					return &((_i++)->second.cap);
				else ++_i;
			}
			return (const Capability *)0;
		}

	private:
		const Membership &_m;
		std::map<uint32_t,CState>::const_iterator _i,_e;
	};
	friend class CapabilityIterator;

	Membership() :
		_lastPushAttempt(0),
		_lastPushedCom(0),
		_blacklistBefore(0),
		_com(),
		_caps(),
		_tags(8)
	{
	}

	/**
	 * Send COM and other credentials to this peer if needed
	 *
	 * This checks last pushed times for our COM and for other credentials and
	 * sends VERB_NETWORK_CREDENTIALS if the recipient might need them.
	 *
	 * @param RR Runtime environment
	 * @param now Current time
	 * @param peerAddress Address of member peer (the one that this Membership describes)
	 * @param nconf My network config
	 * @param cap Capability to send or 0 if none
	 */
	void sendCredentialsIfNeeded(const RuntimeEnvironment *RR,const uint64_t now,const Address &peerAddress,const NetworkConfig &nconf,const Capability *cap);

	/**
	 * @param nconf Our network config
	 * @return True if this peer is allowed on this network at all
	 */
	inline bool isAllowedOnNetwork(const NetworkConfig &nconf) const
	{
		if (nconf.isPublic())
			return true;
		if ((_blacklistBefore)&&(_com.timestamp().first <= _blacklistBefore))
			return false;
		return nconf.com.agreesWith(_com);
	}

	/**
	 * Check whether a capability or tag is expired
	 *
	 * @param cred Credential to check -- must have timestamp() accessor method
	 * @return True if credential is NOT expired
	 */
	template<typename C>
	inline bool isCredentialTimestampValid(const NetworkConfig &nconf,const C &cred) const
	{
		const uint64_t ts = cred.timestamp();
		return ( ( (ts >= nconf.timestamp) || ((nconf.timestamp - ts) <= nconf.credentialTimeToLive) ) && (ts > _blacklistBefore) );
	}

	/**
	 * @param nconf Network configuration
	 * @param id Tag ID
	 * @return Pointer to tag or NULL if not found
	 */
	inline const Tag *getTag(const NetworkConfig &nconf,const uint32_t id) const
	{
		const TState *t = _tags.get(id);
		return ((t) ? (((t->lastReceived != 0)&&(isCredentialTimestampValid(nconf,t->tag))) ? &(t->tag) : (const Tag *)0) : (const Tag *)0);
	}

	/**
	 * @param nconf Network configuration
	 * @param ids Array to store IDs into
	 * @param values Array to store values into
	 * @param maxTags Capacity of ids[] and values[]
	 * @return Number of tags added to arrays
	 */
	inline unsigned int getAllTags(const NetworkConfig &nconf,uint32_t *ids,uint32_t *values,unsigned int maxTags) const
	{
		unsigned int n = 0;
		uint32_t *id = (uint32_t *)0;
		TState *ts = (TState *)0;
		Hashtable<uint32_t,TState>::Iterator i(const_cast<Membership *>(this)->_tags);
		while (i.next(id,ts)) {
			if ((ts->lastReceived)&&(isCredentialTimestampValid(nconf,ts->tag))) {
				if (n >= maxTags)
					return n;
				ids[n] = *id;
				values[n] = ts->tag.value();
			}
		}
		return n;
	}

	/**
	 * @param nconf Network configuration
	 * @param id Capablity ID
	 * @return Pointer to capability or NULL if not found
	 */
	inline const Capability *getCapability(const NetworkConfig &nconf,const uint32_t id) const
	{
		std::map<uint32_t,CState>::const_iterator c(_caps.find(id));
		return ((c != _caps.end()) ? (((c->second.lastReceived != 0)&&(isCredentialTimestampValid(nconf,c->second.cap))) ? &(c->second.cap) : (const Capability *)0) : (const Capability *)0);
	}

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 *
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	int addCredential(const RuntimeEnvironment *RR,const CertificateOfMembership &com);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 *
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	int addCredential(const RuntimeEnvironment *RR,const Tag &tag);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 *
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	int addCredential(const RuntimeEnvironment *RR,const Capability &cap);

	/**
	 * Blacklist COM, tags, and capabilities before this time
	 *
	 * @param ts Blacklist cutoff
	 */
	inline void blacklistBefore(const uint64_t ts)
	{
		_blacklistBefore = ts;
	}

	/**
	 * Clean up old or stale entries
	 *
	 * @return Time of most recent activity in this Membership
	 */
	inline uint64_t clean(const uint64_t now)
	{
		uint64_t lastAct = _lastPushedCom;

		for(std::map<uint32_t,CState>::iterator i(_caps.begin());i!=_caps.end();) {
			const uint64_t la = std::max(i->second.lastPushed,i->second.lastReceived);
			if ((now - la) > ZT_MEMBERSHIP_STATE_EXPIRATION_TIME) {
				_caps.erase(i++);
			} else {
				++i;
				if (la > lastAct)
					lastAct = la;
			}
		}

		uint32_t *i = (uint32_t *)0;
		TState *ts = (TState *)0;
		Hashtable<uint32_t,TState>::Iterator tsi(_tags);
		while (tsi.next(i,ts)) {
			const uint64_t la = std::max(ts->lastPushed,ts->lastReceived);
			if ((now - la) > ZT_MEMBERSHIP_STATE_EXPIRATION_TIME)
				_tags.erase(*i);
			else if (la > lastAct)
				lastAct = la;
		}

		return lastAct;
	}

private:
	// Last time we checked if credential push was needed
	uint64_t _lastPushAttempt;

	// Last time we pushed our COM to this peer
	uint64_t _lastPushedCom;

	// Time before which to blacklist credentials from this peer
	uint64_t _blacklistBefore;

	// COM from this peer
	CertificateOfMembership _com;

	// Capability-related state (we need an ordered container here, hence std::map)
	std::map<uint32_t,CState> _caps;

	// Tag-related state
	Hashtable<uint32_t,TState> _tags;
};

} // namespace ZeroTier

#endif