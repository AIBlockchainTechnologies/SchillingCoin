// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_NET_PROCESSING_H
#define SCHILLINGCOIN_NET_PROCESSING_H

#include <msg_result.h>
#include <net.h>
#include <validationinterface.h>
#include <version.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class AddrMan;
class BanMan;
class CActiveMasternodeManager;
class CBlockIndex;
class CDeterministicMNManager;
class CInv;
class CMasternodeMetaMan;
class CMasternodeSync;
class CNetMsgMaker;
class CScheduler;
class CTransaction;
class CTxMemPool;
class ChainstateManager;
struct LLMQContext;

namespace chainlock {
class Chainlocks;
class ChainlockHandler;
} // namespace chainlock

namespace llmq {
class CInstantSendManager;
class CRecoveredSig;
} // namespace llmq

/** Default for -maxorphantxsize, maximum orphan transaction storage in megabytes. */
static constexpr unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS_SIZE{10};

/** Default number of orphan and replaced transactions retained for compact-block reconstruction. */
static constexpr unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN{100};

static constexpr bool DEFAULT_PEERBLOOMFILTERS{true};
static constexpr bool DEFAULT_PEERBLOCKFILTERS{false};

/** Threshold for marking a peer for discouragement. */
static constexpr int DISCOURAGEMENT_THRESHOLD{100};

/** Maximum number of outstanding compact-block requests for the same block. */
static constexpr unsigned int MAX_CMPCTBLOCKS_INFLIGHT_PER_BLOCK{3};

/**
 * Outcome of authorizing an incoming object against the objects requested
 * from the sending peer.
 */
enum class GetDataResponse {
    REQUESTED,
    LATE,
    UNREQUESTED,
};

struct CNodeStateStats {
    int m_misbehavior_score{0};
    int nSyncHeight{-1};
    int nCommonHeight{-1};
    int m_starting_height{-1};
    std::chrono::microseconds m_ping_wait{0};
    std::vector<int> vHeightInFlight;
    bool m_relay_txs{false};
    uint64_t m_addr_processed{0};
    uint64_t m_addr_rate_limited{0};
    bool m_addr_relay_enabled{false};
    ServiceFlags their_services{NODE_NONE};
};

class PeerManagerInternal
{
public:
    virtual ~PeerManagerInternal() = default;

    virtual void PeerMisbehaving(NodeId nodeid, int howmuch, const std::string& message = "") = 0;
    virtual bool PeerIsBanned(NodeId nodeid) = 0;

    virtual void PeerEraseObjectRequest(NodeId nodeid, const CInv& inv) = 0;
    virtual bool PeerConsumeObjectRequest(NodeId nodeid, const CInv& inv) = 0;
    virtual GetDataResponse PeerConsumeGetDataResponse(NodeId nodeid, const CInv& inv) = 0;
    virtual void PeerForgetObjectRequest(const CInv& inv) = 0;

    virtual void PeerPushInventory(NodeId nodeid, const CInv& inv) = 0;
    virtual void PeerRelayInv(const CInv& inv) = 0;
    virtual void PeerRelayInvFiltered(const CInv& inv, const CTransaction& related_tx) = 0;
    virtual void PeerRelayInvFiltered(const CInv& inv, const uint256& related_tx_hash) = 0;
    virtual void PeerRelayTransaction(const uint256& txid) = 0;
    virtual void PeerRelayRecoveredSig(const llmq::CRecoveredSig& sig, bool proactive_relay) = 0;
    virtual void PeerAskPeersForTransaction(const uint256& txid) = 0;
    virtual size_t PeerGetRequestedObjectCount(NodeId nodeid) const = 0;
    virtual void PeerPostProcessMessage(MessageProcessingResult&& result) = 0;
};

class NetHandler
{
public:
    explicit NetHandler(PeerManagerInternal* peer_manager)
        : m_peer_manager{Assert(peer_manager)}
    {
    }

    virtual ~NetHandler()
    {
        Interrupt();
        Stop();
    }

    virtual void Start() {}
    virtual void Stop() {}
    virtual void Interrupt() {}
    virtual void Schedule(CScheduler& scheduler) {}

    virtual void ProcessMessage(CNode& node, const std::string& msg_type, CDataStream& stream) {}

    virtual bool AlreadyHave(const CInv& inv)
    {
        return false;
    }

    virtual bool ProcessGetData(CNode& node, const CInv& inv, const CNetMsgMaker& msg_maker)
    {
        return false;
    }

protected:
    PeerManagerInternal* m_peer_manager;
};

class PeerManager : public CValidationInterface,
                    public NetEventsInterface,
                    public PeerManagerInternal
{
public:
    static std::unique_ptr<PeerManager> make(
        CConnman& connman,
        AddrMan& addrman,
        BanMan* banman,
        ChainstateManager& chainman,
        CTxMemPool& pool,
        CMasternodeMetaMan& mn_metaman,
        CMasternodeSync& mn_sync,
        const chainlock::Chainlocks& chainlocks,
        chainlock::ChainlockHandler& clhandler,
        CActiveMasternodeManager* nodeman,
        CDeterministicMNManager& dmnman,
        llmq::CInstantSendManager& isman,
        LLMQContext& llmq_ctx,
        bool ignore_incoming_txs);

    ~PeerManager() override = default;

    virtual std::optional<std::string> FetchBlock(
        NodeId peer_id,
        const CBlockIndex& block_index) = 0;

    virtual void StartScheduledTasks(CScheduler& scheduler) = 0;
    virtual bool GetNodeStateStats(NodeId nodeid, CNodeStateStats& stats) const = 0;
    virtual bool IgnoresIncomingTxs() = 0;
    virtual void SendPings() = 0;
    virtual void PushInventory(NodeId nodeid, const CInv& inv) = 0;
    virtual void RelayInv(const CInv& inv) = 0;
    virtual void RelayInv(const CInv& inv, int min_protocol_version) = 0;
    virtual void RelayTransaction(const uint256& txid) = 0;
    virtual void RelayRecoveredSig(const llmq::CRecoveredSig& sig, bool proactive_relay) = 0;
    virtual void SetBestHeight(int height) = 0;
    virtual void UnitTestMisbehaving(NodeId peer_id, int howmuch) = 0;
    virtual void CheckForStaleTipAndEvictPeers() = 0;

    virtual void ProcessMessage(
        CNode& node,
        const std::string& msg_type,
        CDataStream& stream,
        std::chrono::microseconds time_received,
        const std::atomic<bool>& interrupt)
        EXCLUSIVE_LOCKS_REQUIRED(g_msgproc_mutex) = 0;

    virtual void PostProcessMessage(
        MessageProcessingResult&& result,
        NodeId nodeid = -1) = 0;

    virtual void UpdateLastBlockAnnounceTime(NodeId nodeid, int64_t time_in_seconds) = 0;
    virtual bool IsBanned(NodeId nodeid) = 0;
    virtual size_t GetRequestedObjectCount(NodeId nodeid) const = 0;

    virtual void AddExtraHandler(std::unique_ptr<NetHandler>&& handler) = 0;
    virtual void RemoveHandlers() = 0;
    virtual void StartHandlers() = 0;
    virtual void StopHandlers() = 0;
    virtual void InterruptHandlers() = 0;
    virtual void ScheduleHandlers(CScheduler& scheduler) = 0;
};

#endif // SCHILLINGCOIN_NET_PROCESSING_H