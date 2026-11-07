// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "stakeinput.h"
#include "wallet/wallet.h"

bool CSchStake::SetPrevout(const CTxOut& output, const COutPoint& outpoint, const CBlockIndex* pindex)
{
    if (output.IsNull() || outpoint.IsNull() || !pindex)
        return false;

    outputFrom = output;
    outpointFrom = outpoint;
    pindexFrom = pindex;

    return true;
}

bool CSchStake::GetTxOutFrom(CTxOut& out) const
{
    if (outputFrom.IsNull())
        return false;

    out = outputFrom;
    return true;
}

bool CSchStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    if (outpointFrom.IsNull())
        return false;

    txIn = CTxIn(outpointFrom);
    return true;
}

CAmount CSchStake::GetValue() const
{
    return outputFrom.nValue;
}

bool CSchStake::CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKeyKernel = outputFrom.scriptPubKey;

    if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
        return error("%s: failed to parse kernel", __func__);

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH && whichType != TX_COLDSTAKE)
        return error("%s: type=%d (%s) not supported for scriptPubKeyKernel", __func__, whichType, GetTxnOutputType(whichType));

    CScript scriptPubKey;
    CKey key;

    if (whichType == TX_PUBKEYHASH) {
        // if P2PKH check that we have the input private key
        if (!pwallet->GetKey(CKeyID(uint160(vSolutions[0])), key))
            return error("%s: Unable to get staking private key", __func__);

        // convert to P2PK inputs
        scriptPubKey << key.GetPubKey() << OP_CHECKSIG;

    } else {
        // if P2CS, check that we have the coldstaking private key
        if (whichType == TX_COLDSTAKE && !pwallet->GetKey(CKeyID(uint160(vSolutions[0])), key))
            return error("%s: Unable to get cold staking private key", __func__);

        // keep the same script
        scriptPubKey = scriptPubKeyKernel;
    }

    vout.emplace_back(CTxOut(0, scriptPubKey));

    // Calculate if we need to split the output
    if (pwallet->nStakeSplitThreshold > 0) {
        int nSplit = static_cast<int>(nTotal / pwallet->nStakeSplitThreshold);

        if (nSplit > 1) {
            // if nTotal is twice or more of the threshold; create more outputs
            int txSizeMax = MAX_STANDARD_TX_SIZE >> 11; // limit splits to <10% of the max TX size (/2048)

            if (nSplit > txSizeMax)
                nSplit = txSizeMax;

            for (int i = nSplit; i > 1; i--) {
                LogPrintf(
                    "%s: StakeSplit: nTotal = %d; adding output %d of %d\n",
                    __func__,
                    nTotal,
                    (nSplit - i) + 2,
                    nSplit);

                vout.emplace_back(CTxOut(0, scriptPubKey));
            }
        }
    }

    return true;
}

CDataStream CSchStake::GetUniqueness() const
{
    // The historical SCH stake uniqueness is output index followed by txid.
    CDataStream ss(SER_NETWORK, 0);
    ss << outpointFrom.n << outpointFrom.hash;
    return ss;
}

// The block that added the staking UTXO to the candidate chain.
const CBlockIndex* CSchStake::GetIndexFrom() const
{
    return pindexFrom;
}

// Verify stake contextual checks
bool CSchStake::ContextCheck(int nHeight, uint32_t nTime) const
{
    const Consensus::Params& consensus = Params().GetConsensus();

    // Get stake input block time/height.
    const CBlockIndex* pindexFrom = GetIndexFrom();
    if (!pindexFrom)
        return error("%s: unable to get previous index for stake input", __func__);

    const int nHeightBlockFrom = pindexFrom->nHeight;
    const uint32_t nTimeBlockFrom = pindexFrom->nTime;

    // Check that the stake has the required depth/age.
    if (nHeight >= consensus.height_RHF - 1 &&
            !consensus.HasStakeMinAgeOrDepth(
                nHeight,
                nTime,
                nHeightBlockFrom,
                nTimeBlockFrom)) {
        return error(
            "%s : min age violation - height=%d - time=%d, nHeightBlockFrom=%d, nTimeBlockFrom=%d",
            __func__,
            nHeight,
            nTime,
            nHeightBlockFrom,
            nTimeBlockFrom);
    }

    return true;
}