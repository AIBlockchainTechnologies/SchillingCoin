// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Stubbed replacement for the original block of code that implemented:
//   bool RecalculateSCHSupply(int nHeightStart, bool fSkipZsch)
//   bool UpdateZSCHSupply(const CBlock& block, CBlockIndex* pindex)
//
// Purpose: provide ABI-compatible, side-effect-free no-op implementations
// so you can drop this block into the existing translation unit without
// pulling in Zerocoin, UI, disk I/O, or heavy chain traversal dependencies.

#include "stubs/legacy_zerocoin_verify.h"
#include "stubs/legacy_zerocoin_types.h"

#include "chainparams.h"
#include "consensus/consensus.h"
#include "guiinterface.h"        // for ui_interface
#include "init.h"                // for ShutdownRequested()
#include "main.h"
#include "txdb.h"

bool RecalculateSCHSupply(int /*nHeightStart*/, bool /*fSkipZsch*/)
{
    // No-op stub
    //
    // Rationale:
    // - The original function iterated the active chain, read blocks from disk,
    //   computed money-supply deltas, updated block-index fields, wrote to the
    //   block index DB, and displayed UI progress. All of those operations
    //   depend on Zerocoin internals, disk I/O, and UI code that are intentionally
    //   removed in the stubbed build.
    //
    // Behavior:
    // - Return false to indicate the heavy recalculation did not run.
    // - Returning false is conservative: callers that require a successful
    //   recalculation will detect the failure and can handle it explicitly.
    //
    // If you later need callers to proceed as if the recalculation ran,
    // change this to return true only after auditing those callers.
    return false;
}

bool UpdateZSCHSupply(const CBlock& /*block*/, CBlockIndex* /*pindex*/)
{
    // No-op stub
    //
    // Rationale:
    // - The original function updated per-block zSCH supply using Zerocoin mints
    //   and spends, and mutated pindex->mapZerocoinSupply. In a build where
    //   Zerocoin is disabled, mutating pindex or relying on Zerocoin types is
    //   unsafe and unnecessary.
    //
    // Behavior:
    // - Return true to indicate the helper completed successfully without side effects.
    // - Returning true minimizes the chance of higher-level aborts while still
    //   avoiding any state mutation.
    //
    // If a caller depends on pindex->mapZerocoinSupply being modified, either:
    // - update the caller to not require that mutation, or
    // - implement a minimal, well-documented safe mutation here that does not
    //   require Zerocoin types.
    return true;
}