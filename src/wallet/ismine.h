// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_ISMINE_H
#define SCHILLINGCOIN_WALLET_ISMINE_H

#include <consensus/amount.h>
#include <script/ismine.h>

#include <bitset>
#include <type_traits>

namespace wallet {

/** Used for bit flags composed from isminetype values. */
using isminefilter = std::underlying_type_t<isminetype>;

/**
 * Cachable amount subdivided into watch-only and spendable parts.
 */
struct CachableAmount
{
    // NO and ALL are never supposed to be cached.
    std::bitset<ISMINE_ENUM_ELEMENTS> m_cached;
    CAmount m_value[ISMINE_ENUM_ELEMENTS]{};

    void Reset()
    {
        m_cached.reset();
    }

    void Set(const isminefilter filter, const CAmount value)
    {
        m_cached.set(filter);
        m_value[filter] = value;
    }
};

} // namespace wallet

#endif // SCHILLINGCOIN_WALLET_ISMINE_H
