// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_NET_PROCESSING_H
#define SCHILLINGCOIN_NET_PROCESSING_H

#include "net.h"
#include "validationinterface.h"

#include <cstdint>
#include <vector>

/** Default for -maxorphantx, maximum number of orphan transactions kept in memory. */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;

/** Expiration time for orphan transactions in seconds. */
static const int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;

/** Minimum interval between orphan transaction expiration checks in seconds. */
static const int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;

/** Default for -blockspamfilter, use the block spam filter. */
static const bool DEFAULT_BLOCK_SPAM_FILTER = true;

/** Maximum number of block-height entries retained by the block spam filter. */
static const unsigned int DEFAULT_BLOCK_SPAM_FILTER_MAX_SIZE = 100;

/** Maximum average number of received blocks per recorded height. */
static const unsigned int DEFAULT_BLOCK_SPAM_FILTER_MAX_AVG = 10;

/**
 * Validation-interface listener for pre-DIP3 peer-processing state.
 *
 * This listener owns block-source validation handling while SCH retains its
 * existing CNodeSignals-based message-processing lifecycle.
 */
class PeerLogicValidation final : public CValidationInterface
{
private:
    CConnman* const connman;

protected:
    void BlockChecked(
        const CBlock& block,
        const CValidationState& state) override;

public:
    explicit PeerLogicValidation(CConnman* connmanIn);
    ~PeerLogicValidation() override = default;
};

/** Register message-processing callbacks with the network node signals. */
void RegisterNodeSignals(CNodeSignals& nodeSignals);

/** Unregister message-processing callbacks from the network node signals. */
void UnregisterNodeSignals(CNodeSignals& nodeSignals);

/** Statistics maintained for a connected peer. */
struct CNodeStateStats {
    int nMisbehavior;
    int nSyncHeight;
    int nCommonHeight;
    std::vector<int> vHeightInFlight;
};

/** Return the minimum protocol version accepted by SCH. */
int ActiveProtocol();

/** Retrieve statistics from a peer's validation state. */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats& stats);

/** Increase a peer's accumulated misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch);

/** Remove orphan transactions received from a peer. */
void EraseOrphansFor(NodeId peer);

/** Clear all network-processing state during block-index unloading. */
void ClearNetProcessingState();

/** Initialize network-processing state required by block and transaction relay. */
void InitializeNetProcessingState();

/** Limit the orphan transaction pool and return the number evicted. */
unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);

/** Process queued protocol messages received from a peer. */
bool ProcessMessages(CNode* pfrom);

/**
 * Send queued protocol messages to a peer.
 *
 * @param[in] pto          Peer receiving the messages.
 * @param[in] fSendTrickle Whether delayed inventory should be sent now.
 */
bool SendMessages(CNode* pto, bool fSendTrickle);

#endif // SCHILLINGCOIN_NET_PROCESSING_H