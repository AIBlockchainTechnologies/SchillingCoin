// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "coins.h"
#include "db.h"
#include "kernel.h"
#include "legacy/stakemodifier.h"
#include "policy/policy.h"
#include "script/interpreter.h"
#include "stakeinput.h"
#include "util.h"
#include "utilmoneystr.h"
#include "util/system.h"

std::optional<CStakeKernel> CStakeKernel::Create(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int nTimeTx)
{
    if (!pindexPrev || !stakeInput) {
        return std::nullopt;
    }

    const CBlockIndex* pindexFrom{stakeInput->GetIndexFrom()};
    if (!pindexFrom) {
        return std::nullopt;
    }

    CStakeKernel kernel;
    kernel.nTimeBlockFrom = pindexFrom->nTime;
    kernel.stakeUniqueness = stakeInput->GetUniqueness();
    kernel.nTime = nTimeTx;
    kernel.nBits = nBits;
    kernel.stakeValue = stakeInput->GetValue();

    if (!Params().GetConsensus().IsStakeModifierV2(pindexPrev->nHeight + 1)) {
        uint64_t nStakeModifier{0};

        if (!GetOldStakeModifier(stakeInput, pindexPrev, nStakeModifier)) {
            return std::nullopt;
        }

        kernel.stakeModifier << nStakeModifier;
    } else {
        kernel.stakeModifier << pindexPrev->GetStakeModifierV2();
    }

    return kernel;
}

// Return stake kernel hash
uint256 CStakeKernel::GetHash() const
{
    CDataStream ss(stakeModifier);
    ss << nTimeBlockFrom << stakeUniqueness << nTime;
    return Hash(ss);
}

// Check that the kernel hash meets the target required
bool CStakeKernel::CheckKernelHash(bool fSkipLog) const
{
    // Get weighted target
    arith_uint256 bnTarget;
    bnTarget.SetCompact(nBits);
    bnTarget *= (arith_uint256(stakeValue) / 100);

    // Check PoS kernel hash
    const arith_uint256 hashProofOfStake =
        UintToArith256(GetHash());
    const bool res = hashProofOfStake < bnTarget;

    if (!fSkipLog || res) {
        LogPrint(BCLog::POS, "%s : Proof Of Stake:"
                            "\nssUniqueID=%s"
                            "\nnTimeTx=%d"
                            "\nhashProofOfStake=%s"
                            "\nnBits=%d"
                            "\nweight=%d"
                            "\nbnTarget=%s (res: %d)\n\n",
            __func__, HexStr(stakeUniqueness), nTime, hashProofOfStake.GetHex(),
            nBits, stakeValue, bnTarget.GetHex(), res);
    }
    return res;
}

/***********************************
 * PoS Validation
 ***********************************/

static bool LoadStakeInput(const CBlock& block, const CBlockIndex* pindexPrev, const CCoinsViewCache& view, std::unique_ptr<CStakeInput>& stake)
{
    if (!pindexPrev)
        return error("%s: candidate parent index is null", __func__);

    if (block.hashPrevBlock != pindexPrev->GetBlockHash())
        return error("%s: previous block mismatch", __func__);

    if (!block.IsProofOfStake())
        return error("%s: called on non-PoS block", __func__);

    if (block.vtx.size() < 2 || !block.vtx[1]->IsCoinStake())
        return error("%s: malformed proof-of-stake block", __func__);

    if (block.vtx[1]->vin.empty())
        return error("%s: coinstake has no inputs", __func__);

    const COutPoint& prevout{block.vtx[1]->vin[0].prevout};

    Coin coin;
    if (!view.GetCoin(prevout, coin) || coin.IsSpent())
        return error(
            "%s: staking prevout %s:%u is unavailable",
            __func__,
            prevout.hash.ToString(),
            prevout.n);

    if (coin.nHeight > static_cast<uint32_t>(pindexPrev->nHeight))
        return error(
            "%s: staking prevout height %u exceeds candidate parent height %d",
            __func__,
            coin.nHeight,
            pindexPrev->nHeight);

    const CBlockIndex* pindexFrom{
        pindexPrev->GetAncestor(static_cast<int>(coin.nHeight))};

    if (!pindexFrom || pindexFrom->nHeight != static_cast<int>(coin.nHeight))
        return error(
            "%s: unable to resolve stake origin at height %u on candidate branch",
            __func__,
            coin.nHeight);

    auto schStake{std::make_unique<CSchStake>()};

    if (!schStake->SetPrevout(coin.out, prevout, pindexFrom))
        return error("%s: failed to initialize SCH stake input", __func__);

    stake = std::move(schStake);
    return true;
}

/*
 * Stake Check if stakeInput can stake a block on top of pindexPrev
 *
 * @param[in]   pindexPrev      index of the parent of the kernel block
 * @param[in]   stakeInput      input for the coinstake
 * @param[in]   nBits           target difficulty bits
 * @param[in]   nTimeTx         new blocktime
 * @return      bool            true if stake kernel hash meets target protocol
 */
bool Stake(Chainstate& chainstate, const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx)
{
    if (!stakeInput)
        return false;

    const Consensus::Params& consensus = Params().GetConsensus();
    const int nHeightTx = pindexPrev->nHeight + 1;
    const bool fTimeV2 = consensus.IsTimeProtocolV2(nHeightTx);

    if (fTimeV2) {
        // Get the new time slot before performing contextual checks.
        const bool fRegTest = Params().IsRegTestNet();
        nTimeTx = fRegTest ? GetAdjustedTime() : GetCurrentTimeSlot();

        if (nTimeTx <= pindexPrev->nTime && !fRegTest)
            return false;

        // Validate the same timestamp that will be used in the stake kernel.
        if (!stakeInput->ContextCheck(nHeightTx, nTimeTx))
            return false;

        const auto stakeKernel{
            CStakeKernel::Create(
                pindexPrev,
                stakeInput,
                nBits,
                nTimeTx)};

        if (!stakeKernel)
            return false;

        return stakeKernel->CheckKernelHash(true);
    }

    nTimeTx = pindexPrev->nTime;
    unsigned int nTryTime = nTimeTx;
    const unsigned int maxTime = GetAdjustedTime() + 45;

    while (nTryTime < maxTime) {
        // A new block arrived, so stop searching on the previous tip.
        {
            LOCK(cs_main);
            if (chainstate.m_chain.Tip() != pindexPrev)
                break;
        }

        ++nTryTime;

        // Validate the same candidate timestamp that will be hashed.
        if (!stakeInput->ContextCheck(nHeightTx, nTryTime))
            continue;

        const auto stakeKernel{
            CStakeKernel::Create(
                pindexPrev,
                stakeInput,
                nBits,
                nTryTime)};

        if (!stakeKernel || !stakeKernel->CheckKernelHash(true))
            continue;

        nTimeTx = nTryTime;
        return true;
    }

    return false;
}

bool CheckProofOfStake(const CBlock& block, std::string& strError, const CBlockIndex* pindexPrev, const CCoinsViewCache& view)
{
    // Initialize the stake input against the candidate branch UTXO view.
    std::unique_ptr<CStakeInput> stakeInput;
    if (!LoadStakeInput(block, pindexPrev, view, stakeInput)) {
        strError = "stake input initialization failed";
        return false;
    }

    // Stake input contextual checks.
    if (!stakeInput->ContextCheck(pindexPrev->nHeight + 1, block.nTime)) {
        strError = "stake input failing contextual checks";
        return false;
    }

    // Verify the coinstake input signature.
    CTxOut stakePrevout;
    if (!stakeInput->GetTxOutFrom(stakePrevout)) {
        strError = "unable to get stake prevout for coinstake";
        return false;
    }

    const CTransactionRef& tx = block.vtx[1];
    const CTxIn& txin = tx->vin[0];

    ScriptError serror;
    if (!VerifyScript(
            txin.scriptSig,
            stakePrevout.scriptPubKey,
            STANDARD_SCRIPT_VERIFY_FLAGS,
            TransactionSignatureChecker(
                tx.get(),
                0,
                stakePrevout.nValue),
            SIGVERSION_BASE,
            &serror)) {
        strError = strprintf(
            "signature fails: %s",
            serror ? ScriptErrorString(serror) : "");
        return false;
    }

    // Verify Proof Of Stake.
    const auto stakeKernel{
        CStakeKernel::Create(
            pindexPrev,
            stakeInput.get(),
            block.nBits,
            block.nTime)};

    if (!stakeKernel) {
        strError = "stake kernel initialization failed";
        return false;
    }

    if (!stakeKernel->CheckKernelHash()) {
        strError = "kernel hash check fails";
        return false;
    }

    return true;
}

/*
 * GetStakeKernelHash   Return stake kernel of a block
 *
 * @param[out]  hashRet         hash of the kernel (set by this function)
 * @param[in]   block           block with the kernel to return
 * @param[in]   pindexPrev      index of the parent block
 *                              (if nullptr, it will be searched in mapBlockIndex)
 * @return      bool            false if kernel cannot be initialized, true otherwise
 */
bool GetStakeKernelHash(uint256& hashRet, const CBlock& block, const CBlockIndex* pindexPrev)
{
    // Initialize stake input.
    std::unique_ptr<CStakeInput> stakeInput;
    if (!LoadStakeInput(block, pindexPrev, stakeInput))
        return error("%s : stake input initialization failed", __func__);

    const auto stakeKernel{
        CStakeKernel::Create(
            pindexPrev,
            stakeInput.get(),
            block.nBits,
            block.nTime)};

    if (!stakeKernel)
        return error("%s : stake kernel initialization failed", __func__);

    hashRet = stakeKernel->GetHash();
    return true;
}