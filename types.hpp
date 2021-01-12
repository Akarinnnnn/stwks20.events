#pragma once
#include <cstdint>
//extracted from steam public header.

namespace steam
{
	// Steam-specific types. Defined here so this header file can be included in other code bases.
	typedef uint8_t uint8;
	typedef int8_t int8;

	typedef int16_t int16;
	typedef uint16_t uint16;
	typedef int32_t int32;
	typedef uint32_t uint32;
	typedef int64_t int64;
	typedef uint64_t uint64;

	typedef int64 lint64;
	typedef uint64 ulint64;

	typedef intptr_t intp;				// intp is an integer that can accomodate a pointer
	typedef uintptr_t uintp;		// (ie, sizeof(intp) >= sizeof(int) && sizeof(intp) >= sizeof(void *)


	constexpr int k_cubSaltSize = 8;
	typedef	 uint8 Salt_t[k_cubSaltSize];

	//-----------------------------------------------------------------------------
	// GID (GlobalID) stuff
	// This is a globally unique identifier.  It's guaranteed to be unique across all
	// racks and servers for as long as a given universe persists.
	//-----------------------------------------------------------------------------
	// NOTE: for GID parsing/rendering and other utils, see gid.h
	typedef uint64 GID_t;

	constexpr GID_t k_GIDNil = 0xffffffffffffffffull;

	// For convenience, we define a number of types that are just new names for GIDs
	typedef uint64 JobID_t;			// Each Job has a unique ID
	typedef GID_t TxnID_t;			// Each financial transaction has a unique ID

	constexpr GID_t k_TxnIDNil = k_GIDNil;
	constexpr GID_t k_TxnIDUnknown = 0;

	constexpr JobID_t k_JobIDNil = 0xffffffffffffffffull;

	// this is baked into client messages and interfaces as an int, 
	// make sure we never break this.
	typedef uint32 PackageId_t;
	constexpr PackageId_t k_uPackageIdInvalid = 0xFFFFFFFF;

	typedef uint32 BundleId_t;
	constexpr BundleId_t k_uBundleIdInvalid = 0;

	// this is baked into client messages and interfaces as an int, 
	// make sure we never break this.
	typedef uint32 AppId_t;
	constexpr AppId_t k_uAppIdInvalid = 0x0;

	typedef uint64 AssetClassId_t;
	constexpr AssetClassId_t k_ulAssetClassIdInvalid = 0x0;

	typedef uint32 PhysicalItemId_t;
	constexpr PhysicalItemId_t k_uPhysicalItemIdInvalid = 0x0;


	// this is baked into client messages and interfaces as an int, 
	// make sure we never break this.  AppIds and DepotIDs also presently
	// share the same namespace, but since we'd like to change that in the future
	// I've defined it seperately here.
	typedef uint32 DepotId_t;
	constexpr DepotId_t k_uDepotIdInvalid = 0x0;

	// RTime32
	// We use this 32 bit time representing real world time.
	// It offers 1 second resolution beginning on January 1, 1970 (Unix time)
	typedef uint32 RTime32;

	typedef uint32 CellID_t;
	constexpr CellID_t k_uCellIDInvalid = 0xFFFFFFFF;

	// handle to a Steam API call
	typedef uint64 SteamAPICall_t;
	constexpr SteamAPICall_t k_uAPICallInvalid = 0x0;

	typedef uint32 AccountID_t;

	typedef uint32 PartnerId_t;
	constexpr PartnerId_t k_uPartnerIdInvalid = 0;

	// ID for a depot content manifest
	typedef uint64 ManifestId_t;
	constexpr ManifestId_t k_uManifestIdInvalid = 0;

	// ID for cafe sites
	typedef uint64 SiteId_t;
	constexpr SiteId_t k_ulSiteIdInvalid = 0;

	// Party Beacon ID
	typedef uint64 PartyBeaconID_t;
	constexpr PartyBeaconID_t k_ulPartyBeaconIdInvalid = 0;

	enum ESteamIPType
	{
		k_ESteamIPTypeIPv4 = 0,
		k_ESteamIPTypeIPv6 = 1,
	};

#pragma pack( push, 1 )

	struct SteamIPAddress_t
	{
		union {

			uint32			m_unIPv4;		// Host order
			uint8			m_rgubIPv6[16];		// Network order! Same as inaddr_in6.  (0011:2233:4455:6677:8899:aabb:ccdd:eeff)

			// Internal use only
			uint64			m_ipv6Qword[2];	// big endian
		};

		ESteamIPType m_eType;

		bool IsSet() const
		{
			if (k_ESteamIPTypeIPv4 == m_eType)
			{
				return m_unIPv4 != 0;
			}
			else
			{
				return m_ipv6Qword[0] != 0 || m_ipv6Qword[1] != 0;
			}
		}

		static SteamIPAddress_t IPv4Any()
		{
			SteamIPAddress_t ipOut;
			ipOut.m_eType = k_ESteamIPTypeIPv4;
			ipOut.m_unIPv4 = 0;

			return ipOut;
		}

		static SteamIPAddress_t IPv6Any()
		{
			SteamIPAddress_t ipOut;
			ipOut.m_eType = k_ESteamIPTypeIPv6;
			ipOut.m_ipv6Qword[0] = 0;
			ipOut.m_ipv6Qword[1] = 0;

			return ipOut;
		}

		static SteamIPAddress_t IPv4Loopback()
		{
			SteamIPAddress_t ipOut;
			ipOut.m_eType = k_ESteamIPTypeIPv4;
			ipOut.m_unIPv4 = 0x7f000001;

			return ipOut;
		}

		static SteamIPAddress_t IPv6Loopback()
		{
			SteamIPAddress_t ipOut;
			ipOut.m_eType = k_ESteamIPTypeIPv6;
			ipOut.m_ipv6Qword[0] = 0;
			ipOut.m_ipv6Qword[1] = 0;
			ipOut.m_rgubIPv6[15] = 1;

			return ipOut;
		}
	};

#pragma pack( pop )
}