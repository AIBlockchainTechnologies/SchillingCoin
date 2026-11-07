// Copyright (c) 2015-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_DSNOTIFICATIONINTERFACE_H
#define SCHILLINGCOIN_DSNOTIFICATIONINTERFACE_H

#include <validationinterface.h>

class CConnman;
class CDeterministicMNManager;
class ChainstateManager;
class CMasternodeSync;

class CDSNotificationInterface : public CValidationInterface
{
public:
    CDSNotificationInterface() = delete;
    CDSNotificationInterface(const CDSNotificationInterface&) = delete;
    CDSNotificationInterface& operator=(const CDSNotificationInterface&) = delete;
    explicit CDSNotificationInterface(CConnman& connman, CMasternodeSync& mn_sync,
                                      const ChainstateManager& chainman,
                                      CDeterministicMNManager& dmnman);
    virtual ~CDSNotificationInterface();

    // CValidationInterface
    void InitializeCurrentBlockTip(const CBlockIndex* tip, bool ibd) override;

protected:
    // CValidationInterface
    void AcceptedBlockHeader(const CBlockIndex* pindexNew) override;
    void NotifyHeaderTip(const CBlockIndex* pindexNew, bool fInitialDownload) override;
    void SynchronousUpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) override;
    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) override;
    void NotifyMasternodeListChanged(bool undo, const CDeterministicMNList& oldMNList, const CDeterministicMNListDiff& diff) override;

private:
    CConnman& m_connman;
    CMasternodeSync& m_mn_sync;
    const ChainstateManager& m_chainman;
    CDeterministicMNManager& m_dmnman;
};

#endif // SCHILLINGCOIN_DSNOTIFICATIONINTERFACE_H