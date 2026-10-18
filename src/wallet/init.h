// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2021 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_INIT_H
#define SCHILLINGCOIN_WALLET_INIT_H

#include <string>
#include <vector>

//! Return the wallets help message.
std::string GetWalletHelpString(bool showDebug);

//! Wallets parameter interaction
bool WalletParameterInteraction();

//! Responsible for reading and validating the -wallet arguments and verifying the wallet database.
//  This function will perform salvage on the wallet if requested, as long as only one wallet is
//  being loaded (CWallet::ParameterInteraction forbids -salvagewallet, -zapwallettxes or -upgradewallet with multiwallet).
bool WalletVerify();

//! Load the wallet database.
bool InitLoadWallet(const std::vector<std::string>& words);

#endif // SCHILLINGCOIN_WALLET_INIT_H