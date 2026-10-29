// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin developers
// Copyright (c) 2017-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CONSENSUS_AMOUNT_H
#define SCHILLINGCOIN_CONSENSUS_AMOUNT_H

#include <cstdint>

/** Amount in satoshis. Amounts may be negative for intermediate calculations. */
using CAmount = int64_t;

/** Number of satoshis in one SCH. */
static constexpr CAmount COIN = 100000000;

/** Number of satoshis in one hundredth of an SCH. */
static constexpr CAmount CENT = 1000000;

/**
 * Broad maximum amount accepted by SCH amount primitives.
 *
 * Network-specific consensus validation continues to use
 * Consensus::Params::nMaxMoneyOut and Consensus::Params::MoneyRange().
 */
static constexpr CAmount MAX_MONEY = 200000000 * COIN;

/** Check whether an amount is within SCH's broad primitive monetary range. */
inline bool MoneyRange(const CAmount& value)
{
    return value >= 0 && value <= MAX_MONEY;
}

#endif // SCHILLINGCOIN_CONSENSUS_AMOUNT_H