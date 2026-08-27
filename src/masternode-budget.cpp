// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"

#include "addrman.h"
#include "chainparams.h"
#include "masternode-budget.h"
#include "masternode-sync.h"
#include "masternode.h"
#include "masternodeman.h"
#include "util.h"

#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "version.h"
#include "activemasternode.h"

#include <boost/filesystem.hpp>

// -----------------------------------------------------------------------------
// SchillingCoin (SCH) — Budget subsystem inert replacement
// Purpose  : Replace active budget validation and orphan-vote processing with
//            safe, inert stubs after SPORK_9 / SPORK_13 removal.
// Rationale: SPORK_9 and SPORK_13 were removed from the codebase. To avoid
//            linker/runtime errors while fully disabling budget/superblock
//            behaviour, this file provides minimal, deterministic stubs that
//            preserve public symbols and safely drop budget activity.
// -----------------------------------------------------------------------------

CBudgetManager budget;
RecursiveMutex cs_budget;

// define the single shared instance declared as extern in masternode-budget.h
std::map<uint256, int> mapPayment_History;

std::map<uint256, int64_t> askedForSourceProposalOrBudget;
std::vector<CBudgetProposalBroadcast> vecImmatureBudgetProposals;
std::vector<CFinalizedBudgetBroadcast> vecImmatureFinalizedBudgets;

int nSubmittedFinalBudget;

/**
 * IsBudgetCollateralValid
 *
 * Budget subsystem is disabled in SCH builds where SPORK_9/SPORK_13 are removed.
 * This inert implementation always returns false and provides a clear error
 * message. Keeping the function signature intact prevents linker errors and
 * ensures callers receive a deterministic response.
 *
 * Note: If you later re-enable budgets, replace this stub with the original
 * validation logic and remove the explanatory message.
 */
bool IsBudgetCollateralValid(uint256 /*nTxCollateralHash*/, uint256 /*nExpectedHash*/, std::string& strError, int64_t& nTime, int& nConf, bool /*fBudgetFinalization*/)
{
    // Provide deterministic outputs expected by callers
    nTime = 0;
    nConf = 0;

    // Clear any previous error and set a clear, actionable message
    strError = "Budget subsystem disabled: collateral validation not available in this build.";

    // Always return false to indicate collateral is not valid while budgets are disabled.
    return false;
}

/**
 * CheckOrphanVotes
 *
 * When budgets are disabled we should not attempt to process orphan votes.
 * This implementation safely drops orphan votes and finalized-budget orphan
 * votes to avoid unbounded memory growth and unnecessary CPU work.
 *
 * Behavior:
 *  - Logs the drop action for auditability.
 *  - Erases orphan vote entries.
 *
 * Rationale:
 *  - Dropping orphan votes is safe because the budget subsystem is inert and
 *    no proposals/finalized budgets will be activated.
 */
void CBudgetManager::CheckOrphanVotes()
{
    LOCK(cs_budget);

    // If budgets are disabled, orphan votes cannot be resolved. Drop them.
    if (!vecImmatureBudgetProposals.empty()) {
        LogPrint("mnbudget", "CBudgetManager::CheckOrphanVotes - Budget subsystem disabled: clearing %u immature budget proposals\n", (unsigned)vecImmatureBudgetProposals.size());
        vecImmatureBudgetProposals.clear();
    }

    if (!vecImmatureFinalizedBudgets.empty()) {
        LogPrint("mnbudget", "CBudgetManager::CheckOrphanVotes - Budget subsystem disabled: clearing %u immature finalized budgets\n", (unsigned)vecImmatureFinalizedBudgets.size());
        vecImmatureFinalizedBudgets.clear();
    }

    if (!mapOrphanMasternodeBudgetVotes.empty()) {
        LogPrint("mnbudget", "CBudgetManager::CheckOrphanVotes - Budget subsystem disabled: dropping %u orphan masternode budget votes\n", (unsigned)mapOrphanMasternodeBudgetVotes.size());
        mapOrphanMasternodeBudgetVotes.clear();
    }

    if (!mapOrphanFinalizedBudgetVotes.empty()) {
        LogPrint("mnbudget", "CBudgetManager::CheckOrphanVotes - Budget subsystem disabled: dropping %u orphan finalized budget votes\n", (unsigned)mapOrphanFinalizedBudgetVotes.size());
        mapOrphanFinalizedBudgetVotes.clear();
    }

    LogPrint("mnbudget","CBudgetManager::CheckOrphanVotes - Done (budget subsystem disabled)\n");
}

void CBudgetManager::SubmitFinalBudget()
{
    static int nSubmittedHeight = 0; // last height at which we skipped finalization

    // Try to read current height for logging; do not block if cs_main is busy.
    int nCurrentHeight = 0;
    {
        TRY_LOCK(cs_main, locked);
        if (locked && chainActive.Tip()) nCurrentHeight = chainActive.Height();
    }

    if (nCurrentHeight > 0) {
        LogPrint("mnbudget", "CBudgetManager::SubmitFinalBudget - Skipped at height %d: budget subsystem disabled.\n", nCurrentHeight);
        nSubmittedHeight = nCurrentHeight;
    } else {
        LogPrint("mnbudget", "CBudgetManager::SubmitFinalBudget - Skipped: budget subsystem disabled; chain height unavailable.\n");
    }

    // Deterministic no-op: do not create transactions, do not modify maps, do not relay.
    return;
}

//
// CBudgetDB
//

// NOTE: Budget subsystem disabled (SPORK_9/SPORK_13 removed).
// Minimal inert stubs to preserve symbols and avoid disk/network activity.

CBudgetDB::CBudgetDB()
{
    pathDB = GetDataDir() / "budget.dat";
    strMagicMessage = "MasternodeBudget";
}

bool CBudgetDB::Write(const CBudgetManager& /*objToSave*/)
{
    LogPrint("mnbudget", "CBudgetDB::Write - Skipped (budget subsystem disabled)\n");
    return true;
}

CBudgetDB::ReadResult CBudgetDB::Read(CBudgetManager& objToLoad, bool /*fDryRun*/)
{
    LOCK(cs_budget);
    objToLoad.Clear();
    LogPrint("mnbudget", "CBudgetDB::Read - Skipped (budget subsystem disabled)\n");
    return Ok;
}

void DumpBudgets()
{
    LogPrint("mnbudget", "DumpBudgets - Skipped (budget subsystem disabled)\n");
}

bool CBudgetManager::AddFinalizedBudget(CFinalizedBudget& /*finalizedBudget*/)
{
    return false;
}

bool CBudgetManager::AddProposal(CBudgetProposal& /*budgetProposal*/)
{
    return false;
}

void CBudgetManager::CheckAndRemove()
{
    LOCK(cs_budget);
    mapFinalizedBudgets.clear();
    mapProposals.clear();
    mapSeenMasternodeBudgetProposals.clear();
    mapSeenFinalizedBudgets.clear();
    mapSeenMasternodeBudgetVotes.clear();
    mapSeenFinalizedBudgetVotes.clear();
    mapOrphanMasternodeBudgetVotes.clear();
    mapOrphanFinalizedBudgetVotes.clear();
    vecImmatureBudgetProposals.clear();
    vecImmatureFinalizedBudgets.clear();

    LogPrint("mnbudget", "CBudgetManager::CheckAndRemove - Cleared budget state (budget subsystem disabled)\n");
}

void CBudgetManager::FillBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake)
{
    // No budget payees when budgets are disabled. Leave txNew unchanged.
    LogPrint("mnbudget", "CBudgetManager::FillBlockPayee - Skipped (budget subsystem disabled)\n");
    return;
}

CFinalizedBudget* CBudgetManager::FindFinalizedBudget(uint256 /*nHash*/)
{
    // Budgets disabled — never return a finalized budget.
    return nullptr;
}

CBudgetProposal* CBudgetManager::FindProposal(const std::string& /*strProposalName*/)
{
    // Budgets disabled — never return a proposal by name.
    return nullptr;
}

CBudgetProposal* CBudgetManager::FindProposal(uint256 /*nHash*/)
{
    LOCK(cs_budget);
    // Budgets disabled — never return a proposal by hash.
    return nullptr;
}

bool CBudgetManager::IsBudgetPaymentBlock(int /*nBlockHeight*/)
{
    // Budgets disabled — no block is a budget payment block.
    return false;
}

TrxValidationStatus CBudgetManager::IsTransactionValid(const CTransaction& /*txNew*/, int /*nBlockHeight*/)
{
    // Budgets disabled — treat transactions as not valid for budget payouts.
    return TrxValidationStatus::InValid;
}

std::vector<CBudgetProposal*> CBudgetManager::GetAllProposals()
{
    LOCK(cs_budget);
    // Budgets disabled — return empty list.
    return std::vector<CBudgetProposal*>();
}

struct sortProposalsByVotes {
    bool operator()(const std::pair<CBudgetProposal*, int>& left, const std::pair<CBudgetProposal*, int>& right)
    {
        if (left.second != right.second)
            return (left.second > right.second);
        return (left.first->nFeeTXHash > right.first->nFeeTXHash);
    }
};

std::vector<CBudgetProposal*> CBudgetManager::GetBudget()
{
    LOCK(cs_budget);
    // Budgets disabled — no proposals to return.
    return std::vector<CBudgetProposal*>();
}

struct sortFinalizedBudgetsByVotes {
    bool operator()(const std::pair<CFinalizedBudget*, int>& left, const std::pair<CFinalizedBudget*, int>& right)
    {
        if (left.second != right.second)
            return left.second > right.second;
        return (left.first->nFeeTXHash > right.first->nFeeTXHash);
    }
};

std::vector<CFinalizedBudget*> CBudgetManager::GetFinalizedBudgets()
{
    LOCK(cs_budget);
    // Budgets disabled — return empty list.
    return std::vector<CFinalizedBudget*>();
}

std::string CBudgetManager::GetRequiredPaymentsString(int /*nBlockHeight*/)
{
    // Budgets disabled — no required payments.
    return std::string("unknown-budget");
}

CAmount CBudgetManager::GetTotalBudget(int /*nHeight*/)
{
    // Budgets disabled — total budget is zero.
    return 0;
}

// NOTE: Budget subsystem disabled. NewBlock is inert to avoid budget activity.
void CBudgetManager::NewBlock()
{
    // Try to acquire budget lock briefly; if busy, skip.
    TRY_LOCK(cs_budget, fBudgetNewBlock);
    if (!fBudgetNewBlock) return;

    // Do not run budget sync, submission, or voting logic when budgets are disabled.
    // Keep minimal housekeeping: clear transient orphan maps to avoid memory growth.
    {
        LOCK(cs_budget);
        askedForSourceProposalOrBudget.clear();
        vecImmatureBudgetProposals.clear();
        vecImmatureFinalizedBudgets.clear();
        mapOrphanMasternodeBudgetVotes.clear();
        mapOrphanFinalizedBudgetVotes.clear();
    }

    LogPrint("mnbudget", "CBudgetManager::NewBlock - Skipped budget processing (disabled)\n");
    return;
}

void CBudgetManager::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    // lite mode is not supported
    if (fLiteMode) return;

    // Do not process budget messages until the node is fully synced with the chain.
    if (!masternodeSync.IsBlockchainSynced()) return;

    // Keep the budget lock for compatibility with callers that expect it.
    LOCK(cs_budget);

    // If budgets are disabled, ignore all budget-related protocol messages.
    // This prevents any wallet, disk, or voting side effects while keeping
    // the message handlers present for protocol compatibility.
    if (strCommand == "mnvs" || strCommand == "mprop" || strCommand == "mvote" ||
        strCommand == "fbs"  || strCommand == "fbvote") {
        LogPrint("mnbudget", "CBudgetManager::ProcessMessage - Ignoring budget message %s (budget subsystem disabled)\n", strCommand);
        return;
    }

    // If other non-budget messages are added here in future, handle them below.
    // Currently there are no non-budget message handlers in this module.
}

bool CBudgetManager::PropExists(uint256 nHash)
{
    LOCK(cs_budget);
    // Preserve original semantics: check whether a proposal with this hash exists.
    // If budgets are disabled and maps are cleared, this will correctly return false.
    return mapProposals.count(nHash) != 0;
}

void CBudgetManager::ResetSync()
{
    LOCK(cs_budget);
    // Clear any transient sync markers and orphan containers.
    askedForSourceProposalOrBudget.clear();
    mapOrphanMasternodeBudgetVotes.clear();
    mapOrphanFinalizedBudgetVotes.clear();
    // Keep seen maps intact but mark no votes as synced (no-op).
    LogPrint("mnbudget", "CBudgetManager::ResetSync - Skipped detailed sync reset (budget disabled)\n");
}

void CBudgetManager::MarkSynced()
{
    LOCK(cs_budget);
    // No-op: budgets disabled so nothing to mark as synced.
    LogPrint("mnbudget", "CBudgetManager::MarkSynced - Skipped (budget disabled)\n");
}

void CBudgetManager::Sync(CNode* /*pfrom*/, uint256 /*nProp*/, bool /*fPartial*/)
{
    // Budgets disabled — do not send inventory or messages.
    LogPrint("mnbudget", "CBudgetManager::Sync - Ignored (budget subsystem disabled)\n");
    return;
}

bool CBudgetManager::UpdateProposal(CBudgetVote& /*vote*/, CNode* /*pfrom*/, std::string& strError)
{
    // Budgets disabled — refuse updates and do not queue orphan votes.
    strError = "Budget subsystem disabled in this build";
    return false;
}

bool CBudgetManager::UpdateFinalizedBudget(CFinalizedBudgetVote& /*vote*/, CNode* /*pfrom*/, std::string& strError)
{
    // Budgets disabled — refuse updates and do not queue orphan votes.
    strError = "Budget subsystem disabled in this build";
    return false;
}

CBudgetProposal::CBudgetProposal()
{
    strProposalName = "unknown";
    nBlockStart = 0;
    nBlockEnd = 0;
    nAmount = 0;
    nTime = 0;
    // Mark invalid so other code treats proposals as inactive while budgets are disabled.
    fValid = false;
}

CBudgetProposal::CBudgetProposal(std::string strProposalNameIn, std::string strURLIn, int nBlockStartIn, int nBlockEndIn, CScript addressIn, CAmount nAmountIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;
    nBlockStart = nBlockStartIn;
    nBlockEnd = nBlockEndIn;
    address = addressIn;
    nAmount = nAmountIn;
    nFeeTXHash = nFeeTXHashIn;
    // Mark invalid while budget subsystem is disabled
    fValid = false;
}

CBudgetProposal::CBudgetProposal(const CBudgetProposal& other)
{
    strProposalName = other.strProposalName;
    strURL = other.strURL;
    nBlockStart = other.nBlockStart;
    nBlockEnd = other.nBlockEnd;
    address = other.address;
    nAmount = other.nAmount;
    nTime = other.nTime;
    nFeeTXHash = other.nFeeTXHash;
    mapVotes = other.mapVotes;
    // Preserve invalid state for safety
    fValid = false;
}

bool CBudgetProposal::IsValid(std::string& strError, bool /*fCheckCollateral*/)
{
    // Budgets disabled: always report invalid with a clear message.
    strError = "Budget subsystem disabled in this build";
    return false;
}

bool CBudgetProposal::IsEstablished()
{
    // Treat proposals as not established while budgets are disabled.
    return false;
}

bool CBudgetProposal::IsPassing(const CBlockIndex* /*pindexPrev*/, int /*nBlockStartBudget*/, int /*nBlockEndBudget*/, int /*mnCount*/)
{
    // Budgets disabled: never passing.
    return false;
}

bool CBudgetProposal::AddOrUpdateVote(CBudgetVote& /*vote*/, std::string& strError)
{
    // Do not accept or store votes while budgets are disabled.
    strError = "Budget subsystem disabled in this build";
    return false;
}

void CBudgetProposal::CleanAndRemove()
{
    // Mark all votes invalid (masternode voting disabled).
    for (auto &kv : mapVotes) {
        kv.second.fValid = false;
    }
}

double CBudgetProposal::GetRatio()
{
    // No active votes while budgets are disabled.
    return 0.0;
}

int CBudgetProposal::GetYeas() const
{
    return 0;
}

int CBudgetProposal::GetNays() const
{
    return 0;
}

int CBudgetProposal::GetAbstains() const
{
    return 0;
}

int CBudgetProposal::GetBlockStartCycle()
{
    // Return normalized cycle start if available, otherwise 0.
    const int blocks = Params().GetConsensus().nBudgetCycleBlocks;
    if (blocks <= 0) return 0;
    return nBlockStart - (nBlockStart % blocks);
}

int CBudgetProposal::GetBlockCurrentCycle()
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return -1;
    const int blocks = Params().GetConsensus().nBudgetCycleBlocks;
    if (blocks <= 0) return -1;
    if (pindexPrev->nHeight >= GetBlockEndCycle()) return -1;
    return pindexPrev->nHeight - (pindexPrev->nHeight % blocks);
}

int CBudgetProposal::GetBlockEndCycle()
{
    // Preserve stored end value.
    return nBlockEnd;
}

int CBudgetProposal::GetTotalPaymentCount()
{
    const int blocks = Params().GetConsensus().nBudgetCycleBlocks;
    if (blocks <= 0) return 0;
    return (GetBlockEndCycle() - GetBlockStartCycle()) / blocks;
}

int CBudgetProposal::GetRemainingPaymentCount()
{
    int current = GetBlockCurrentCycle();
    if (current < 0) return 0;
    const int blocks = Params().GetConsensus().nBudgetCycleBlocks;
    if (blocks <= 0) return 0;
    int nPayments = (GetBlockEndCycle() - current) / blocks - 1;
    return std::max(0, std::min(nPayments, GetTotalPaymentCount()));
}

/* Broadcast helpers (inert) */
CBudgetProposalBroadcast::CBudgetProposalBroadcast(std::string strProposalNameIn, std::string strURLIn, int nPaymentCount, CScript addressIn, CAmount nAmountIn, int nBlockStartIn, uint256 nFeeTXHashIn)
{
    strProposalName = strProposalNameIn;
    strURL = strURLIn;
    nBlockStart = nBlockStartIn;

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nCycleStart = (nBlocksPerCycle > 0) ? (nBlockStart - nBlockStart % nBlocksPerCycle) : nBlockStart;

    // Keep same formula but do not rely on active budget processing.
    nBlockEnd = nCycleStart + ((nBlocksPerCycle > 0) ? ((nBlocksPerCycle + 1) * nPaymentCount) : nPaymentCount);

    address = addressIn;
    nAmount = nAmountIn;
    nFeeTXHash = nFeeTXHashIn;
}

void CBudgetProposalBroadcast::Relay()
{
    LogPrint("mnbudget", "CBudgetProposalBroadcast::Relay - Skipped (budget subsystem disabled)\n");
}

CBudgetVote::CBudgetVote() :
        CSignedMessage(),
        fValid(false),
        fSynced(false),
        vin(),
        nProposalHash(),
        nVote(VOTE_ABSTAIN),
        nTime(0)
{ }

CBudgetVote::CBudgetVote(CTxIn vinIn, uint256 nProposalHashIn, int nVoteIn) :
        CSignedMessage(),
        fValid(false),
        fSynced(false),
        vin(vinIn),
        nProposalHash(nProposalHashIn),
        nVote(nVoteIn)
{
    nTime = GetAdjustedTime();
}

CBudgetVote::CBudgetVote(const CBudgetVote& other)
{
    fValid        = false;
    fSynced       = other.fSynced;
    vin           = other.vin;
    nProposalHash = other.nProposalHash;
    nVote         = other.nVote;
    nTime         = other.nTime;
    vchSig        = other.vchSig;
    nMessVersion  = other.nMessVersion;
}

CBudgetVote::CBudgetVote(CBudgetVote&& other) noexcept
{
    fValid        = false;
    fSynced       = other.fSynced;
    vin           = std::move(other.vin);
    nProposalHash = other.nProposalHash;
    nVote         = other.nVote;
    nTime         = other.nTime;
    vchSig        = std::move(other.vchSig);
    nMessVersion  = other.nMessVersion;
}

CBudgetVote& CBudgetVote::operator=(const CBudgetVote& other)
{
    if (this != &other) {
        fValid        = false;
        fSynced       = other.fSynced;
        vin           = other.vin;
        nProposalHash = other.nProposalHash;
        nVote         = other.nVote;
        nTime         = other.nTime;
        vchSig        = other.vchSig;
        nMessVersion  = other.nMessVersion;
    }
    return *this;
}

CBudgetVote& CBudgetVote::operator=(CBudgetVote&& other) noexcept
{
    if (this != &other) {
        fValid        = other.fValid;
        fSynced       = other.fSynced;
        vin           = std::move(other.vin);
        nProposalHash = other.nProposalHash;
        nVote         = other.nVote;
        nTime         = other.nTime;
        vchSig        = std::move(other.vchSig);
        nMessVersion  = other.nMessVersion;
    }
    return *this;
}

void CBudgetVote::Relay()
{
    // Budget relays disabled.
    LogPrint("mnbudget", "CBudgetVote::Relay - Skipped (budget subsystem disabled)\n");
}

uint256 CBudgetVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << nProposalHash;
    ss << nVote;
    ss << nTime;
    return ss.GetHash();
}

std::string CBudgetVote::GetStrMessage() const
{
    return vin.prevout.ToStringShort() + nProposalHash.ToString() +
            std::to_string(nVote) + std::to_string(nTime);
}

CFinalizedBudget::CFinalizedBudget() :
        fAutoChecked(false),
        fValid(false),
        strBudgetName(""),
        nBlockStart(0),
        vecBudgetPayments(),
        mapVotes(),
        nFeeTXHash(),
        nTime(0)
{ }

CFinalizedBudget::CFinalizedBudget(const CFinalizedBudget& other) :
        fAutoChecked(false),
        fValid(false),
        strBudgetName(other.strBudgetName),
        nBlockStart(other.nBlockStart),
        vecBudgetPayments(other.vecBudgetPayments),
        mapVotes(other.mapVotes),
        nFeeTXHash(other.nFeeTXHash),
        nTime(other.nTime)
{ }

bool CFinalizedBudget::AddOrUpdateVote(CFinalizedBudgetVote& vote, std::string& strError)
{
    // Budgets disabled: do not accept or store votes.
    LOCK(cs_budget);
    strError = "Budget subsystem disabled in this build";
    return false;
}

// Sort budget proposals by hash
struct sortProposalsByHash  {
    bool operator()(const CBudgetProposal* left, const CBudgetProposal* right)
    {
        return (left->GetHash() < right->GetHash());
    }
};

// Sort budget payments by hash
struct sortPaymentsByHash  {
    bool operator()(const CTxBudgetPayment& left, const CTxBudgetPayment& right)
    {
        return (left.nProposalHash < right.nProposalHash);
    }
};

void CFinalizedBudget::CheckAndVote()
{
    // Budgets disabled: skip auto-check and voting.
    LogPrint("mnbudget", "CFinalizedBudget::CheckAndVote - Skipped (budget subsystem disabled)\n");
    return;
}

// Remove votes from masternodes which are not valid/existent anymore
void CFinalizedBudget::CleanAndRemove()
{
    // Mark all votes invalid to avoid any further processing.
    for (auto &kv : mapVotes) {
        kv.second.fValid = false;
    }
}

CAmount CFinalizedBudget::GetTotalPayout()
{
    // Budgets disabled: report zero payout.
    return 0;
}

std::string CFinalizedBudget::GetProposals()
{
    LOCK(cs_budget);
    // Budgets disabled: no proposals to report.
    return std::string();
}

std::string CFinalizedBudget::GetStatus()
{
    // Budgets disabled: return a clear status string.
    return std::string("disabled");
}

bool CFinalizedBudget::IsValid(std::string& strError, bool /*fCheckCollateral*/)
{
    // Budgets disabled: always report invalid with a clear message.
    strError = "Budget subsystem disabled in this build";
    return false;
}

bool CFinalizedBudget::IsPaidAlready(uint256 /*nProposalHash*/, int /*nBlockHeight*/)
{
    // Budgets disabled: treat as not paid (allow normal masternode payment flow).
    return false;
}

TrxValidationStatus CFinalizedBudget::IsTransactionValid(const CTransaction& /*txNew*/, int /*nBlockHeight*/)
{
    // Budgets disabled: no budget transactions are considered valid.
    LogPrint("mnbudget", "CFinalizedBudget::IsTransactionValid - Skipped (budget subsystem disabled)\n");
    return TrxValidationStatus::InValid;
}

void CFinalizedBudget::SubmitVote()
{
    // Voting disabled: do not sign or relay votes.
    LogPrint("mnbudget", "CFinalizedBudget::SubmitVote - Skipped (budget subsystem disabled)\n");
    return;
}

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast() :
        CFinalizedBudget()
{ }

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(const CFinalizedBudget& other) :
        CFinalizedBudget(other)
{ }

CFinalizedBudgetBroadcast::CFinalizedBudgetBroadcast(std::string strBudgetNameIn,
                                                     int nBlockStartIn,
                                                     std::vector<CTxBudgetPayment> vecBudgetPaymentsIn,
                                                     uint256 nFeeTXHashIn)
{
    strBudgetName = strBudgetNameIn;
    nBlockStart = nBlockStartIn;
    vecBudgetPayments = vecBudgetPaymentsIn;
    nFeeTXHash = nFeeTXHashIn;
}

void CFinalizedBudgetBroadcast::Relay()
{
    // Relaying disabled.
    LogPrint("mnbudget", "CFinalizedBudgetBroadcast::Relay - Skipped (budget subsystem disabled)\n");
}

CFinalizedBudgetVote::CFinalizedBudgetVote() :
        CSignedMessage(),
        fValid(false),
        fSynced(false),
        vin(),
        nBudgetHash(),
        nTime(0)
{ }

CFinalizedBudgetVote::CFinalizedBudgetVote(CTxIn vinIn, uint256 nBudgetHashIn) :
        CSignedMessage(),
        fValid(false),
        fSynced(false),
        vin(vinIn),
        nBudgetHash(nBudgetHashIn)
{
    nTime = GetAdjustedTime();
}

CFinalizedBudgetVote::CFinalizedBudgetVote(const CFinalizedBudgetVote& other)
{
    fValid       = false;
    fSynced      = other.fSynced;
    vin          = other.vin;
    nBudgetHash  = other.nBudgetHash;
    nTime        = other.nTime;
    vchSig       = other.vchSig;
    nMessVersion = other.nMessVersion;
}

CFinalizedBudgetVote::CFinalizedBudgetVote(CFinalizedBudgetVote&& other) noexcept
{
    fValid       = false;
    fSynced      = other.fSynced;
    vin          = std::move(other.vin);
    nBudgetHash  = other.nBudgetHash;
    nTime        = other.nTime;
    vchSig       = std::move(other.vchSig);
    nMessVersion = other.nMessVersion;
}

CFinalizedBudgetVote& CFinalizedBudgetVote::operator=(const CFinalizedBudgetVote& other)
{
    if (this != &other) {
        fValid       = false;
        fSynced      = other.fSynced;
        vin          = other.vin;
        nBudgetHash  = other.nBudgetHash;
        nTime        = other.nTime;
        vchSig       = other.vchSig;
        nMessVersion = other.nMessVersion;
    }
    return *this;
}

CFinalizedBudgetVote& CFinalizedBudgetVote::operator=(CFinalizedBudgetVote&& other) noexcept
{
    if (this != &other) {
        fValid       = other.fValid;
        fSynced      = other.fSynced;
        vin          = std::move(other.vin);
        nBudgetHash  = other.nBudgetHash;
        nTime        = other.nTime;
        vchSig       = std::move(other.vchSig);
        nMessVersion = other.nMessVersion;
    }
    return *this;
}

void CFinalizedBudgetVote::Relay()
{
    // Relaying disabled.
    LogPrint("mnbudget", "CFinalizedBudgetVote::Relay - Skipped (budget subsystem disabled)\n");
}

uint256 CFinalizedBudgetVote::GetHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << vin;
    ss << nBudgetHash;
    ss << nTime;
    return ss.GetHash();
}

std::string CFinalizedBudgetVote::GetStrMessage() const
{
    return vin.prevout.ToStringShort() + nBudgetHash.ToString() + std::to_string(nTime);
}

std::string CBudgetManager::ToString() const
{
    std::ostringstream info;

    info << "Proposals: " << (int)mapProposals.size()
         << ", Budgets: " << (int)mapFinalizedBudgets.size()
         << ", Seen Budgets: " << (int)mapSeenMasternodeBudgetProposals.size()
         << ", Seen Budget Votes: " << (int)mapSeenMasternodeBudgetVotes.size()
         << ", Seen Final Budgets: " << (int)mapSeenFinalizedBudgets.size()
         << ", Seen Final Budget Votes: " << (int)mapSeenFinalizedBudgetVotes.size();

    return info.str();
}