// Copyright (c) 2017 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_WALLETUTIL_H
#define SCHILLINGCOIN_WALLET_WALLETUTIL_H

#include "operationresult.h"
#include "util/fs.h"

//! Get the path of the wallet directory.
fs::path GetWalletDir();
//! Verify the wallet db's path
OperationResult VerifyWalletPath(const std::string& walletFile);

#endif // SCHILLINGCOIN_WALLET_WALLETUTIL_H