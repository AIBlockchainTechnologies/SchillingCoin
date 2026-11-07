// Copyright (c) 2014-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_MASTERNODE_UTILS_H
#define SCHILLINGCOIN_MASTERNODE_UTILS_H

class CConnman;
class CDeterministicMNManager;
class CMasternodeSync;
class CMasternodeUtils
{
public:
    static void DoMaintenance(CConnman& connman, CDeterministicMNManager& dmnman, const CMasternodeSync& mn_sync);
};

#endif // SCHILLINGCOIN_MASTERNODE_UTILS_H