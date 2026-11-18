// Copyright (c) 2017-2021 The Particl Core developers
// Copyright (c) 2026 The SchillingCon developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_RPCHDWALLET_H
#define SCHILLINGCOIN_WALLET_RPCHDWALLET_H

class CRPCCommand;
class CHDWallet;
class CRPCTable;

void EnsureWalletIsUnlocked(const CHDWallet *pwallet);

Span<const CRPCCommand> GetHDWalletRPCCommands();

void RegisterNonWalletRPCCommands(CRPCTable& t);

#endif // SCHILLINGCOIN_WALLET_RPCHDWALLET_H
