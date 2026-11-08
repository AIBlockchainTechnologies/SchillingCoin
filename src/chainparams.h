// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CHAINPARAMS_H
#define SCHILLINGCOIN_CHAINPARAMS_H

#include <kernel/chainparams.h> // IWYU pragma: export

#include <memory>

class ArgsManager;

/**
 * Add chain-parameter command-line options.
 */
void SetupChainParamsOptions(ArgsManager& argsman);

/**
 * Creates and returns parameters for the chosen chain type.
 */
std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, ChainType chain);

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams& Params();

/**
 * Sets the parameters returned by Params() for the selected chain.
 */
void SelectParams(ChainType chain);

#endif // SCHILLINGCOIN_CHAINPARAMS_H