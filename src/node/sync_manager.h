// Copyright (c) 2014-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_NODE_SYNC_MANAGER_H
#define SCHILLINGCOIN_NODE_SYNC_MANAGER_H

#include <masternode/sync.h>
#include <net_processing.h>

class CMasternodeSync;
class CNetFulfilledRequestManager;

class SyncManager final : public NetHandler
{
public:
    SyncManager(PeerManagerInternal* peer_manager, CMasternodeSync& node_sync, CConnman& connman,
                CNetFulfilledRequestManager& netfulfilledman) :
        NetHandler(peer_manager),
        m_node_sync(node_sync),
        m_connman(connman),
        m_netfulfilledman(netfulfilledman)
    {
    }

    void Schedule(CScheduler& scheduler) override;

private:
    void ProcessTick();

    CMasternodeSync& m_node_sync;
    CConnman& m_connman;
    CNetFulfilledRequestManager& m_netfulfilledman;
};

class NodeSyncNotifierImpl final : public NodeSyncNotifier
{
public:
    NodeSyncNotifierImpl(CConnman& connman, CNetFulfilledRequestManager& netfulfilledman) :
        m_connman(connman),
        m_netfulfilledman(netfulfilledman)
    {
    }

    void SyncReset() override;
    void SyncFinished() override;

private:
    CConnman& m_connman;
    CNetFulfilledRequestManager& m_netfulfilledman;
};

#endif // SCHILLINGCOIN_NODE_SYNC_MANAGER_H
