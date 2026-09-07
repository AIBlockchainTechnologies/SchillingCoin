// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validationinterface.h"
#include "primitives/transaction.h"   // defines CTransaction
#include "primitives/block.h"         // defines CBlock
#include "uint256.h"                  // defines uint256
#include "chain.h"                    // defines CBlockIndex, CBlockLocator
#include "consensus/validation.h"     // defines CValidationState

#include <map>
#include <vector>
#include <utility>
#include <cassert>

#include <boost/signals2/connection.hpp>

static CMainSignals g_signals;

/**
 * Store the connections returned by boost::signals2::signal::connect()
 * so we can reliably disconnect the exact slots later.
 */
static std::map<CValidationInterface*, std::vector<boost::signals2::connection>> g_connections;

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    if (!pwalletIn) return;

    std::vector<boost::signals2::connection> conns;
    conns.reserve(12);

    // Connect UpdatedBlockTip: void(const CBlockIndex*)
    conns.push_back(g_signals.UpdatedBlockTip.connect(
        [pwalletIn](const CBlockIndex* pindex) {
            pwalletIn->UpdatedBlockTip(pindex);
        }
    ));

    // Connect SyncTransaction: void(const CTransaction&, const CBlock*)
    conns.push_back(g_signals.SyncTransaction.connect(
        [pwalletIn](const CTransaction& tx, const CBlock* pblock) {
            pwalletIn->SyncTransaction(tx, pblock);
        }
    ));

    // Connect NotifyTransactionLock: void(const CTransaction &)
    conns.push_back(g_signals.NotifyTransactionLock.connect(
        [pwalletIn](const CTransaction& tx) {
            pwalletIn->NotifyTransactionLock(tx);
        }
    ));

    // Connect UpdatedTransaction: bool(const uint256 &)
    // Note: the signal expects a bool return; we forward the call and ignore the return value here.
    conns.push_back(g_signals.UpdatedTransaction.connect(
        [pwalletIn](const uint256& hash) -> bool {
            // Forward and return the wallet's response if needed by callers of the signal.
            return pwalletIn->UpdatedTransaction(hash);
        }
    ));

    // Connect SetBestChain: void(const CBlockLocator &)
    conns.push_back(g_signals.SetBestChain.connect(
        [pwalletIn](const CBlockLocator& locator) {
            pwalletIn->SetBestChain(locator);
        }
    ));

    // Connect Inventory: void(const uint256 &)
    conns.push_back(g_signals.Inventory.connect(
        [pwalletIn](const uint256& hash) {
            pwalletIn->Inventory(hash);
        }
    ));

    // Connect Broadcast: void()
    conns.push_back(g_signals.Broadcast.connect(
        [pwalletIn]() {
            pwalletIn->ResendWalletTransactions();
        }
    ));

    // Connect BlockChecked: void(const CBlock&, const CValidationState&)
    conns.push_back(g_signals.BlockChecked.connect(
        [pwalletIn](const CBlock& block, const CValidationState& state) {
            pwalletIn->BlockChecked(block, state);
        }
    ));

    // Connect BlockFound: void(const uint256 &)
    conns.push_back(g_signals.BlockFound.connect(
        [pwalletIn](const uint256& hash) {
            pwalletIn->ResetRequestCount(hash);
        }
    ));

    // Store the connections so UnregisterValidationInterface can disconnect them precisely.
    g_connections.emplace(pwalletIn, std::move(conns));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    if (!pwalletIn) return;

    auto it = g_connections.find(pwalletIn);
    if (it != g_connections.end()) {
        for (auto &conn : it->second) {
            if (conn.connected()) conn.disconnect();
        }
        g_connections.erase(it);
    } else {
        // Fallback: if we don't have stored connections (older code paths),
        // attempt to disconnect matching slots by disconnecting all slots for the signals.
        // This is conservative; prefer stored connections for precise disconnection.
        g_signals.BlockFound.disconnect_all_slots();
        g_signals.BlockChecked.disconnect_all_slots();
        g_signals.Broadcast.disconnect_all_slots();
        g_signals.Inventory.disconnect_all_slots();
        g_signals.SetBestChain.disconnect_all_slots();
        g_signals.UpdatedTransaction.disconnect_all_slots();
        g_signals.NotifyTransactionLock.disconnect_all_slots();
        g_signals.SyncTransaction.disconnect_all_slots();
        g_signals.UpdatedBlockTip.disconnect_all_slots();
    }
}

void UnregisterAllValidationInterfaces() {
    // Disconnect all stored connections
    for (auto &entry : g_connections) {
        for (auto &conn : entry.second) {
            if (conn.connected()) conn.disconnect();
        }
    }
    g_connections.clear();

    // Also clear any remaining slots on the global signals to ensure a clean state.
    g_signals.BlockFound.disconnect_all_slots();
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
    g_signals.SetBestChain.disconnect_all_slots();
    g_signals.UpdatedTransaction.disconnect_all_slots();
    g_signals.NotifyTransactionLock.disconnect_all_slots();
    g_signals.SyncTransaction.disconnect_all_slots();
    g_signals.UpdatedBlockTip.disconnect_all_slots();
}

void SyncWithWallets(const CTransaction &tx, const CBlock *pblock) {
    g_signals.SyncTransaction(tx, pblock);
}