// Copyright (c) 2024-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/sync_manager.h>

#include <chainparams.h>
#include <logging.h>
#include <masternode/sync.h>
#include <net.h>
#include <netfulfilledman.h>
#include <netmessagemaker.h>
#include <node/interface_ui.h>
#include <scheduler.h>
#include <shutdown.h>
#include <util/system.h>
#include <util/time.h>

#include <cassert>
#include <chrono>
#include <string>

void SyncManager::Schedule(CScheduler& scheduler)
{
    scheduler.scheduleEvery(
        [this]() {
            if (ShutdownRequested()) return;
            ProcessTick();
        },
        std::chrono::seconds{1});
}

void SyncManager::ProcessTick()
{
    assert(m_netfulfilledman.IsValid());

    static int nTick{0};
    ++nTick;

    static const int64_t nSyncStart{
        TicksSinceEpoch<std::chrono::milliseconds>(SystemClock::now())};
    static const std::string strAllow{strprintf("allow-sync-%lld", nSyncStart)};
    static int64_t nTimeLastProcess{GetTime()};

    if (!Params().IsMockableChain() &&
        GetTime() - nTimeLastProcess > 60 * 60 &&
        !m_connman.IsActiveMasternode()) {
        LogPrintf("Sync Tick -- WARNING: no actions for too long, restarting sync...\n");
        m_node_sync.Reset(true);
        nTimeLastProcess = GetTime();
        return;
    }

    if (GetTime() - nTimeLastProcess < MASTERNODE_SYNC_TICK_SECONDS) {
        return;
    }

    nTimeLastProcess = GetTime();

    if (m_node_sync.IsSynced()) {
        return;
    }

    const CConnman::NodesSnapshot nodes_snapshot{
        m_connman,
        CConnman::FullyConnectedOnly};

    LogPrint(BCLog::MNSYNC,
             "Sync Tick -- nTick %d asset_id %d nTriedPeerCount %d\n",
             nTick,
             m_node_sync.GetAssetID(),
             m_node_sync.GetAttempt());

    for (CNode* peer : nodes_snapshot.Nodes()) {
        if (!peer->CanRelay() ||
            (m_connman.IsActiveMasternode() && peer->IsInboundConn())) {
            continue;
        }

        if ((peer->HasPermission(NetPermissionFlags::NoBan) ||
             peer->IsManualConn()) &&
            !m_netfulfilledman.HasFulfilledRequest(peer->addr, strAllow)) {
            m_netfulfilledman.RemoveAllFulfilledRequests(peer->addr);
            m_netfulfilledman.AddFulfilledRequest(peer->addr, strAllow);
            LogPrintf("Sync Tick -- skipping mnsync restrictions for peer=%d\n",
                      peer->GetId());
        }

        if (m_netfulfilledman.HasFulfilledRequest(peer->addr, "full-sync")) {
            peer->fDisconnect = true;
            LogPrintf("Sync Tick -- disconnecting from recently synced peer=%d\n",
                      peer->GetId());
            continue;
        }

        const int64_t sync_timeout{
            nodes_snapshot.Nodes().size() > 3
                ? MASTERNODE_SYNC_TICK_SECONDS
                : MASTERNODE_SYNC_TIMEOUT_SECONDS};

        if (!m_node_sync.IsReachedBestHeader() ||
            GetTime() - m_node_sync.GetLastBump() <= sync_timeout) {
            continue;
        }

        m_node_sync.SwitchToNextAsset();

        if (gArgs.GetBoolArg("-syncmempool", DEFAULT_SYNC_MEMPOOL)) {
            for (CNode* mempool_peer : nodes_snapshot.Nodes()) {
                const bool requested_earlier{
                    m_netfulfilledman.HasFulfilledRequest(
                        mempool_peer->addr,
                        "mempool-sync")};

                if (mempool_peer->IsInboundConn() ||
                    requested_earlier ||
                    mempool_peer->IsBlockRelayOnly()) {
                    continue;
                }

                m_netfulfilledman.AddFulfilledRequest(
                    mempool_peer->addr,
                    "mempool-sync");

                CNetMsgMaker msg_maker{mempool_peer->GetCommonVersion()};
                m_connman.PushMessage(
                    mempool_peer,
                    msg_maker.Make(NetMsgType::MEMPOOL));

                LogPrint(BCLog::MNSYNC,
                         "Sync Tick -- syncing mempool from peer=%d\n",
                         mempool_peer->GetId());
            }
        }

        return;
    }
}

void NodeSyncNotifierImpl::SyncReset()
{
    uiInterface.NotifyAdditionalDataSyncProgressChanged(-1);
}

void NodeSyncNotifierImpl::SyncFinished()
{
    assert(m_netfulfilledman.IsValid());

    uiInterface.NotifyAdditionalDataSyncProgressChanged(1);

    m_connman.ForEachNode(
        CConnman::AllNodes,
        [this](const CNode* peer) {
            m_netfulfilledman.AddFulfilledRequest(peer->addr, "full-sync");
        });
}