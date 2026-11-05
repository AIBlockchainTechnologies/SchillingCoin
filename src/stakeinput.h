// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_STAKEINPUT_H
#define SCHILLINGCOIN_STAKEINPUT_H

#include "chain.h"
#include "streams.h"
#include "uint256.h"

class CKeyStore;
class CWallet;
class CWalletTx;

class CStakeInput
{
protected:
    explicit CStakeInput(const CBlockIndex* pindexFromIn = nullptr)
        : pindexFrom(pindexFromIn)
    {
    }

    const CBlockIndex* pindexFrom{nullptr};

public:
    virtual ~CStakeInput() = default;

    virtual const CBlockIndex* GetIndexFrom() const = 0;
    virtual bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = UINT256_ZERO) = 0;
    virtual bool GetTxFrom(CTransaction& tx) const = 0;
    virtual bool GetTxOutFrom(CTxOut& out) const = 0;
    virtual CAmount GetValue() const = 0;
    virtual bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) = 0;
    virtual CDataStream GetUniqueness() const = 0;
    virtual bool ContextCheck(int nHeight, uint32_t nTime) const = 0;
};

class CSchStake : public CStakeInput
{
private:
    CTransaction txFrom{CTransaction()};
    CTxOut outputFrom{};
    COutPoint outpointFrom{};

public:
    CSchStake() = default;

    CSchStake(const CTransaction& txPrev, unsigned int n, const CBlockIndex* pindex)
        : CStakeInput(pindex),
          txFrom(txPrev),
          outputFrom(txPrev.vout[n]),
          outpointFrom(txPrev.GetHash(), n)
    {
    }

    CSchStake(const CTxOut& output, const COutPoint& outpoint, const CBlockIndex* pindex)
        : CStakeInput(pindex),
          outputFrom(output),
          outpointFrom(outpoint)
    {
    }

    bool SetPrevout(const CTransaction& txPrev, unsigned int n, const CBlockIndex* pindex);

    bool SetPrevout(const CTxOut& output, const COutPoint& outpoint, const CBlockIndex* pindex);

    const CBlockIndex* GetIndexFrom() const override;
    bool GetTxFrom(CTransaction& tx) const override;
    bool GetTxOutFrom(CTxOut& out) const override;
    CAmount GetValue() const override;
    CDataStream GetUniqueness() const override;
    bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = UINT256_ZERO) override;
    bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) override;
    bool ContextCheck(int nHeight, uint32_t nTime) const override;
};

#endif // SCHILLINGCOIN_STAKEINPUT_H