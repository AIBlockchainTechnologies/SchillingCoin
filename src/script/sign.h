// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_SCRIPT_SIGN_H
#define SCHILLINGCOIN_SCRIPT_SIGN_H

#include "script/interpreter.h"

class CKeyID;
class CKeyStore;
class CScript;
class CTransaction;

struct CMutableTransaction;

/** Virtual base class for signature creators. */
class BaseSignatureCreator
{
protected:
    const CKeyStore* keystore;

public:
    explicit BaseSignatureCreator(const CKeyStore* keystoreIn) :
        keystore(keystoreIn)
    {
    }

    const CKeyStore& KeyStore() const
    {
        return *keystore;
    }

    virtual ~BaseSignatureCreator()
    {
    }

    virtual const BaseSignatureChecker& Checker() const = 0;

    /** Create a singular non-script signature. */
    virtual bool CreateSig(
        std::vector<unsigned char>& vchSig,
        const CKeyID& keyid,
        const CScript& scriptCode,
        SigVersion sigversion) const = 0;
};

/** A signature creator for immutable transactions. */
class TransactionSignatureCreator : public BaseSignatureCreator
{
private:
    const CTransaction* txTo;
    unsigned int nIn;
    int nHashType;
    CAmount amount;
    const TransactionSignatureChecker checker;

public:
    TransactionSignatureCreator(
        const CKeyStore* keystoreIn,
        const CTransaction* txToIn,
        unsigned int nInIn,
        const CAmount& amountIn,
        int nHashTypeIn = SIGHASH_ALL);

    const BaseSignatureChecker& Checker() const override
    {
        return checker;
    }

    bool CreateSig(
        std::vector<unsigned char>& vchSig,
        const CKeyID& keyid,
        const CScript& scriptCode,
        SigVersion sigversion) const override;
};

/** A signature creator for mutable transactions. */
class MutableTransactionSignatureCreator : public TransactionSignatureCreator
{
private:
    const CTransaction tx;

public:
    MutableTransactionSignatureCreator(
        const CKeyStore* keystoreIn,
        const CMutableTransaction* txToIn,
        unsigned int nInIn,
        const CAmount& amountIn,
        int nHashTypeIn) :
        TransactionSignatureCreator(
            keystoreIn,
            &tx,
            nInIn,
            amountIn,
            nHashTypeIn),
        tx(*txToIn)
    {
    }
};

/** A signature creator that produces dummy signatures. */
class DummySignatureCreator : public BaseSignatureCreator
{
public:
    explicit DummySignatureCreator(const CKeyStore* keystoreIn) :
        BaseSignatureCreator(keystoreIn)
    {
    }

    const BaseSignatureChecker& Checker() const override;

    bool CreateSig(
        std::vector<unsigned char>& vchSig,
        const CKeyID& keyid,
        const CScript& scriptCode,
        SigVersion sigversion) const override;
};

struct SignatureData
{
    CScript scriptSig;

    SignatureData()
    {
    }

    explicit SignatureData(const CScript& script) :
        scriptSig(script)
    {
    }
};

/** Produce a script signature using a generic signature creator. */
bool ProduceSignature(
    const BaseSignatureCreator& creator,
    const CScript& fromPubKey,
    SignatureData& sigdata,
    SigVersion sigversion,
    bool fColdStake = false,
    ScriptError* serror = nullptr);

/** Produce a script signature for a transaction. */
bool SignSignature(
    const CKeyStore& keystore,
    const CScript& fromPubKey,
    CMutableTransaction& txTo,
    unsigned int nIn,
    const CAmount& amount,
    int nHashType = SIGHASH_ALL,
    bool fColdStake = false);

bool SignSignature(
    const CKeyStore& keystore,
    const CTransaction& txFrom,
    CMutableTransaction& txTo,
    unsigned int nIn,
    int nHashType = SIGHASH_ALL,
    bool fColdStake = false);

/**
 * Combine two script signatures using a generic signature checker,
 * intelligently handling incomplete signatures and OP_0 placeholders.
 */
SignatureData CombineSignatures(
    const CScript& scriptPubKey,
    const BaseSignatureChecker& checker,
    const SignatureData& scriptSig1,
    const SignatureData& scriptSig2);

/** Extract signature data from a transaction and insert updated data. */
SignatureData DataFromTransaction(
    const CMutableTransaction& tx,
    unsigned int nIn);

void UpdateTransaction(
    CMutableTransaction& tx,
    unsigned int nIn,
    const SignatureData& data);

#endif // SCHILLINGCOIN_SCRIPT_SIGN_H