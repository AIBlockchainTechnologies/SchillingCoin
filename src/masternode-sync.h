// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODE_SYNC_H
#define MASTERNODE_SYNC_H

#include <atomic>

#define MASTERNODE_SYNC_INITIAL 0
#define MASTERNODE_SYNC_LIST 2
#define MASTERNODE_SYNC_MNW 3
#define MASTERNODE_SYNC_FAILED 998
#define MASTERNODE_SYNC_FINISHED 999

#define MASTERNODE_SYNC_TIMEOUT 5
#define MASTERNODE_SYNC_THRESHOLD 2

class CMasternodeSync;
extern CMasternodeSync masternodeSync;

class CMasternodeSync
{
public:
    std::map<uint256, int> mapSeenSyncMNB;
    std::map<uint256, int> mapSeenSyncMNW;

    int64_t lastMasternodeList;
    int64_t lastMasternodeWinner;
    int64_t lastFailure;
    int nCountFailures;

    std::atomic<int64_t> lastProcess;
    std::atomic<bool> fBlockchainSynced;

    int sumMasternodeList;
    int sumMasternodeWinner;

    int countMasternodeList;
    int countMasternodeWinner;

    int RequestedMasternodeAssets;
    int RequestedMasternodeAttempt;

    int64_t nAssetSyncStarted;

    CMasternodeSync();

    void AddedMasternodeList(uint256 hash);
    void AddedMasternodeWinner(uint256 hash);
    void GetNextAsset();
    std::string GetSyncStatus();
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void Reset();
    void Process();
    bool IsSynced();
    bool NotCompleted();
    bool IsMasternodeListSynced();
    bool IsBlockchainSynced();
    void ClearFulfilledRequest();
};

#endif