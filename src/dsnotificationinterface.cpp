// Copyright (c) 2014-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dsnotificationinterface.h>

#include <evo/deterministicmns.h>
#include <evo/mnauth.h>
#include <masternode/sync.h>
#include <validation.h>

CDSNotificationInterface::CDSNotificationInterface(CConnman& connman, CMasternodeSync& mn_sync,
                                                   const ChainstateManager& chainman,
                                                   CDeterministicMNManager& dmnman) :
    m_connman{connman},
    m_mn_sync{mn_sync},
    m_chainman{chainman},
    m_dmnman{dmnman}
{
}

CDSNotificationInterface::~CDSNotificationInterface() = default;

void CDSNotificationInterface::InitializeCurrentBlockTip(const CBlockIndex* tip, bool ibd)
{
    SynchronousUpdatedBlockTip(tip, nullptr, ibd);
    UpdatedBlockTip(tip, nullptr, ibd);
}

void CDSNotificationInterface::AcceptedBlockHeader(const CBlockIndex *pindexNew)
{
    m_mn_sync.AcceptedBlockHeader(pindexNew);
}

void CDSNotificationInterface::NotifyHeaderTip(const CBlockIndex *pindexNew, bool fInitialDownload)
{
    m_mn_sync.NotifyHeaderTip(pindexNew, fInitialDownload);
}

void CDSNotificationInterface::SynchronousUpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload)
{
    if (pindexNew == pindexFork) // blocks were disconnected without any new ones
        return;

    m_dmnman.UpdatedBlockTip(pindexNew);
}

void CDSNotificationInterface::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload)
{
    if (pindexNew == pindexFork) // blocks were disconnected without any new ones
        return;

    m_mn_sync.UpdatedBlockTip(WITH_LOCK(::cs_main, return m_chainman.m_best_header), pindexNew, fInitialDownload);
}

void CDSNotificationInterface::NotifyMasternodeListChanged(bool undo, const CDeterministicMNList& oldMNList, const CDeterministicMNListDiff& diff)
{
    CMNAuth::NotifyMasternodeListChanged(undo, oldMNList, diff, m_connman);
}