// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_VERSION_H
#define SCHILLINGCOIN_VERSION_H

/**
 * Network protocol versioning.
 *
 * The initial modernized SCH binary continues advertising the historical
 * protocol version so that it remains compatible with legacy SCH peers.
 * Modern capabilities remain unavailable until SCH deliberately advances
 * PROTOCOL_VERSION for the deterministic-masternode activation release.
 */
static const int PROTOCOL_VERSION = 70005;

//! Initial protocol version before version/verack negotiation.
static const int INIT_PROTO_VERSION = 209;

//! In this version, getheaders was introduced.
static const int GETHEADERS_VERSION = 70000;

//! Continue accepting historical SCH peers during the pre-DIP2 transition.
static const int MIN_PEER_PROTO_VERSION = 70003;

//! Historical SCH masternode-announcement compatibility.
static const int MIN_PEER_MNANNOUNCE = 70003;

//! ADDRv2 was introduced in this protocol version.
static const int ADDRV2_PROTO_VERSION = 70223;

//! The basic BLS scheme was introduced in this protocol version.
static const int BLS_SCHEME_PROTO_VERSION = 70225;

//! Deterministic masternode types were introduced in this protocol version.
static const int DMN_TYPE_PROTO_VERSION = 70227;

//! Versioned simplified masternode-list entries were introduced in this protocol version.
static const int SMNLE_VERSIONED_PROTO_VERSION = 70228;

//! Simplified masternode-list diff serialization moved its version field first.
static const int MNLISTDIFF_VERSION_ORDER = 70229;

//! ChainLock information was added to simplified masternode-list diffs.
static const int MNLISTDIFF_CHAINLOCKS_PROTO_VERSION = 70230;

//! Legacy InstantSend lock messages and inventory were retired in this protocol version.
static const int NO_LEGACY_ISLOCK_PROTO_VERSION = 70231;

//! The compressed HEADERS2 limit was increased and transaction reconciliation enabled.
static const int INCREASE_MAX_HEADERS2_VERSION = 70235;

//! BIP324 transport with Dash-family short IDs was introduced in this protocol version.
static const int BIP324_DASH_BASELINE_VERSION = 70235;

//! Efficient quorum-rotation-information construction was introduced in this protocol version.
static const int EFFICIENT_QRINFO_VERSION = 70236;

//! InstantSend cycle hashes switched to the quorum base block in this protocol version.
static const int ISDLOCK_CYCLEHASH_UPDATE_VERSION = 70237;

//! Re-propagation of stale quorum final commitments was restricted in this protocol version.
static const int QFCOMMIT_STALE_REPROP_BAN_VERSION = 70239;

//! Temporary imported Platform short-ID gate.
//! Remove this constant with the remaining Platform protocol path.
static const int PLATFORMBAN_V2_SHORT_ID_VERSION = 70240;

//! Temporary imported AssetUnlock inventory gate.
//! Remove this constant with the remaining AssetUnlock protocol path.
static const int ASSET_UNLOCK_INV_VERSION = 70242;

//! nTime was added to CAddress in this protocol version.
static const int CADDR_TIME_VERSION = 31402;

//! BIP31 pong messages are enabled after this protocol version.
static const int BIP0031_VERSION = 60000;

//! The mempool command and enhanced getdata behavior begin with this version.
static const int MEMPOOL_GD_VERSION = 60002;

//! Filter commands require NODE_BLOOM at and after this protocol version.
static const int NO_BLOOM_VERSION = 70000;


#endif // SCHILLINGCOIN_VERSION_H