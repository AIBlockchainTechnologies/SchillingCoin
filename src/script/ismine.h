// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Particl Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_SCRIPT_ISMINE_H
#define SCHILLINGCOIN_SCRIPT_ISMINE_H

#include <script/standard.h>

#include <cstdint>
#include <type_traits>

class CKeyStore;
class CScript;

/**
 * IsMine() return codes.
 *
 * SCH retains PIVX-style P2CS ownership classifications together with the
 * Particl-derived used-address, hardware-device, and watch-cold-stake flags.
 */
enum isminetype : unsigned int {
    ISMINE_NO                    = 0,
    ISMINE_WATCH_ONLY_           = 1U << 0,
    ISMINE_SPENDABLE             = 1U << 1,

    //! The wallet owns the staking key of a P2CS output.
    ISMINE_COLD                  = 1U << 2,

    //! The wallet owns the spending key of a P2CS output.
    ISMINE_SPENDABLE_DELEGATED   = 1U << 3,

    ISMINE_USED                  = 1U << 4,
    ISMINE_HARDWARE_DEVICE       = 1U << 6,

    //! Watch-only P2CS ownership.
    ISMINE_WATCH_COLDSTAKE       = 1U << 7,

    ISMINE_WATCH_ONLY =
        ISMINE_WATCH_ONLY_ | ISMINE_WATCH_COLDSTAKE,

    ISMINE_SPENDABLE_ALL =
        ISMINE_SPENDABLE | ISMINE_SPENDABLE_DELEGATED,

    ISMINE_SPENDABLE_STAKEABLE =
        ISMINE_COLD | ISMINE_SPENDABLE_DELEGATED,

    ISMINE_ALL =
        ISMINE_WATCH_ONLY |
        ISMINE_SPENDABLE |
        ISMINE_COLD |
        ISMINE_SPENDABLE_DELEGATED,

    ISMINE_ALL_USED = ISMINE_ALL | ISMINE_USED,
    ISMINE_ENUM_ELEMENTS,
};

/** Used for bit flags composed from isminetype values. */
using isminefilter = std::underlying_type_t<isminetype>;

isminetype IsMine(const CKeyStore& keystore, const CScript& script_pub_key);
isminetype IsMine(const CKeyStore& keystore, const CTxDestination& destination);

#endif // SCHILLINGCOIN_SCRIPT_ISMINE_H
