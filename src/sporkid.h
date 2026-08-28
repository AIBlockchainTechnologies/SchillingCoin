// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPORKID_H
#define SPORKID_H

/*
    Don't ever reuse these IDs for other sporks...
    - This would result in old clients getting confused about which spork is for what.

    The following legacy sporks have been permanently removed from SCH:
      * SwiftTX sporks
          SPORK_2_SWIFTTX;
          SPORK_3_SWIFTTX_BLOCK_FILTERING;
          SPORK_5_MAX_VALUE;

      * Zerocoin sporks
          SPORK_16_ZEROCOIN_MAINTENANCE_MODE;
          SPORK_18_ZEROCOIN_PUBLICSPEND_V4;

      * Governance / Superblock sporks
          SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT;
          SPORK_13_ENABLE_SUPERBLOCKS;

      * Masternode payment enforcement spork
          SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT;

      * Cold staking enforcement spork
          SPORK_17_COLDSTAKING_ENFORCEMENT;

    Rationale:
    - SPORK_8 was retired because its behavior is now statically hard‑coded into SCH.
    - SPORK_17 and the other listed sporks are permanently removed or replaced by
      hard‑coded behavior; the chain no longer relies on runtime spork toggles for
      these features.

    Important:
    - These numeric IDs must never be reused for new features.
    - Reusing an ID risks older clients misinterpreting the meaning of a spork
      message and behaving unpredictably.
*/

enum SporkId : int32_t {
    SPORK_14_NEW_PROTOCOL_ENFORCEMENT           = 10013,
    SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2         = 10014,

    SPORK_INVALID                               = -1
};

// Default values
struct CSporkDef
{
    CSporkDef(): sporkId(SPORK_INVALID), defaultValue(0) {}
    CSporkDef(SporkId id, int64_t val, std::string n): sporkId(id), defaultValue(val), name(n) {}
    SporkId sporkId;
    int64_t defaultValue;
    std::string name;
};

#endif