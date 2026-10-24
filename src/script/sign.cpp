// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/sign.h"

#include "key.h"
#include "keystore.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "uint256.h"
#include "util/system.h"

typedef std::vector<unsigned char> valtype;

TransactionSignatureCreator::TransactionSignatureCreator(const CKeyStore* keystoreIn, const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, int nHashTypeIn) : BaseSignatureCreator(keystoreIn), txTo(txToIn), nIn(nInIn), nHashType(nHashTypeIn), amount(amountIn), checker(txTo, nIn, amountIn)
{
}

bool TransactionSignatureCreator::CreateSig(std::vector<unsigned char>& vchSig, const CKeyID& address, const CScript& scriptCode, SigVersion sigversion) const
{
    CKey key;
    if (!keystore->GetKey(address, key))
        return false;

    uint256 hash;
    try {
        hash = SignatureHash(
            scriptCode,
            *txTo,
            nIn,
            nHashType,
            amount,
            sigversion);
    } catch (const std::logic_error&) {
        return false;
    }

    if (!key.Sign(hash, vchSig))
        return false;

    vchSig.push_back(static_cast<unsigned char>(nHashType));
    return true;
}

static bool Sign1(const CKeyID& address, const BaseSignatureCreator& creator, const CScript& scriptCode, std::vector<valtype>& ret, SigVersion sigversion)
{
    std::vector<unsigned char> vchSig;
    if (!creator.CreateSig(
            vchSig,
            address,
            scriptCode,
            sigversion)) {
        return false;
    }

    ret.emplace_back(vchSig);
    return true;
}

static bool SignN(const std::vector<valtype>& multisigdata, const BaseSignatureCreator& creator, const CScript& scriptCode, std::vector<valtype>& ret, SigVersion sigversion)
{
    int nSigned = 0;
    const int nRequired = multisigdata.front()[0];

    for (unsigned int i = 1;
         i < multisigdata.size() - 1 && nSigned < nRequired;
         ++i) {
        const valtype& pubkey = multisigdata[i];
        const CKeyID keyID = CPubKey(pubkey).GetID();

        if (Sign1(
                keyID,
                creator,
                scriptCode,
                ret,
                sigversion)) {
            ++nSigned;
        }
    }

    return nSigned == nRequired;
}

/**
 * Sign scriptPubKey using a signature made with creator.
 *
 * Signatures are returned in ret, unless whichTypeRet is TX_SCRIPTHASH,
 * in which case ret contains the redemption script.
 */
static bool SignStep(const BaseSignatureCreator& creator, const CScript& scriptPubKey, std::vector<valtype>& ret, txnouttype& whichTypeRet, SigVersion sigversion, bool fColdStake)
{
    CScript scriptRet;
    ret.clear();

    std::vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichTypeRet, vSolutions))
        return false;

    CKeyID keyID;

    switch (whichTypeRet) {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
    case TX_ZEROCOINMINT:
        return false;

    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        return Sign1(
            keyID,
            creator,
            scriptPubKey,
            ret,
            sigversion);

    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));

        if (!Sign1(
                keyID,
                creator,
                scriptPubKey,
                ret,
                sigversion)) {
            return false;
        }

        {
            CPubKey pubkey;
            if (!creator.KeyStore().GetPubKey(keyID, pubkey)) {
                return error(
                    "%s : Unable to get public key from keyID",
                    __func__);
            }

            ret.emplace_back(ToByteVector(pubkey));
        }

        return true;

    case TX_SCRIPTHASH:
        if (creator.KeyStore().GetCScript(
                uint160(vSolutions[0]),
                scriptRet)) {
            ret.emplace_back(
                scriptRet.begin(),
                scriptRet.end());
            return true;
        }

        return false;

    case TX_MULTISIG:
        ret.emplace_back();
        return SignN(
            vSolutions,
            creator,
            scriptPubKey,
            ret,
            sigversion);

    case TX_COLDSTAKE:
        if (fColdStake) {
            // Sign with the cold-staker key.
            keyID = CKeyID(uint160(vSolutions[0]));
        } else {
            // Sign with the owner key.
            keyID = CKeyID(uint160(vSolutions[1]));
        }

        if (!Sign1(
                keyID,
                creator,
                scriptPubKey,
                ret,
                sigversion)) {
            return error(
                "%s: failed to sign with the %s key",
                __func__,
                fColdStake ? "cold staker" : "owner");
        }

        {
            CPubKey pubkey;
            if (!creator.KeyStore().GetPubKey(keyID, pubkey)) {
                return error(
                    "%s : Unable to get public key from keyID",
                    __func__);
            }

            valtype selector;
            selector.emplace_back(
                fColdStake
                    ? static_cast<unsigned char>(OP_TRUE)
                    : static_cast<unsigned char>(OP_FALSE));

            ret.emplace_back(selector);
            ret.emplace_back(ToByteVector(pubkey));
        }

        return true;
    }

    return false;
}

static CScript PushAll(const std::vector<valtype>& values)
{
    CScript result;

    for (const valtype& value : values) {
        if (value.empty()) {
            result << OP_0;
        } else if (value.size() == 1 &&
                   value[0] >= 1 &&
                   value[0] <= 16) {
            result << CScript::EncodeOP_N(value[0]);
        } else {
            result << value;
        }
    }

    return result;
}

bool ProduceSignature(const BaseSignatureCreator& creator, const CScript& fromPubKey, SignatureData& sigdata, SigVersion sigversion, bool fColdStake, ScriptError* serror)
{
    CScript script = fromPubKey;
    bool solved = true;
    std::vector<valtype> result;
    txnouttype whichType;

    solved = SignStep(
        creator,
        script,
        result,
        whichType,
        sigversion,
        fColdStake);

    CScript subscript;

    if (solved && whichType == TX_SCRIPTHASH) {
        // Solver returns the subscript that needs to be evaluated.
        // The final scriptSig contains the signatures followed by the
        // serialized redemption script.
        script = subscript = CScript(
            result[0].begin(),
            result[0].end());

        solved =
            SignStep(
                creator,
                script,
                result,
                whichType,
                sigversion,
                fColdStake) &&
            whichType != TX_SCRIPTHASH;

        result.emplace_back(
            subscript.begin(),
            subscript.end());
    }

    sigdata.scriptSig = PushAll(result);

    return solved &&
           VerifyScript(
               sigdata.scriptSig,
               fromPubKey,
               STANDARD_SCRIPT_VERIFY_FLAGS,
               creator.Checker(),
               sigversion,
               serror);
}

SignatureData DataFromTransaction(const CMutableTransaction& tx, unsigned int nIn)
{
    assert(tx.vin.size() > nIn);

    SignatureData data;
    data.scriptSig = tx.vin[nIn].scriptSig;
    return data;
}

void UpdateTransaction(CMutableTransaction& tx, unsigned int nIn, const SignatureData& data)
{
    assert(tx.vin.size() > nIn);
    tx.vin[nIn].scriptSig = data.scriptSig;
}

bool SignSignature(const CKeyStore& keystore, const CScript& fromPubKey, CMutableTransaction& txTo, unsigned int nIn, const CAmount& amount, int nHashType, bool fColdStake)
{
    assert(nIn < txTo.vin.size());

    const CTransaction txToConst(txTo);

    TransactionSignatureCreator creator(
        &keystore,
        &txToConst,
        nIn,
        amount,
        nHashType);

    SignatureData sigdata;

    const bool ret = ProduceSignature(
        creator,
        fromPubKey,
        sigdata,
        SIGVERSION_BASE,
        fColdStake);

    UpdateTransaction(txTo, nIn, sigdata);
    return ret;
}

bool SignSignature(const CKeyStore& keystore, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, int nHashType, bool fColdStake)
{
    assert(nIn < txTo.vin.size());

    const CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());

    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    return SignSignature(
        keystore,
        txout.scriptPubKey,
        txTo,
        nIn,
        txout.nValue,
        nHashType,
        fColdStake);
}

static std::vector<valtype> CombineMultisig(const CScript& scriptPubKey, const BaseSignatureChecker& checker, const std::vector<valtype>& vSolutions, const std::vector<valtype>& sigs1, const std::vector<valtype>& sigs2, SigVersion sigversion)
{
    std::set<valtype> allSigs;

    for (const valtype& sig : sigs1) {
        if (!sig.empty())
            allSigs.insert(sig);
    }

    for (const valtype& sig : sigs2) {
        if (!sig.empty())
            allSigs.insert(sig);
    }

    assert(vSolutions.size() > 1);

    const unsigned int nSigsRequired = vSolutions.front()[0];
    const unsigned int nPubKeys = vSolutions.size() - 2;
    std::map<valtype, valtype> signatures;

    for (const valtype& sig : allSigs) {
        for (unsigned int i = 0; i < nPubKeys; ++i) {
            const valtype& pubkey = vSolutions[i + 1];

            if (signatures.count(pubkey))
                continue;

            if (checker.CheckSig(
                    sig,
                    pubkey,
                    scriptPubKey,
                    sigversion)) {
                signatures[pubkey] = sig;
                break;
            }
        }
    }

    unsigned int nSigsHave = 0;
    std::vector<valtype> result;

    // CHECKMULTISIG pop-one-too-many workaround.
    result.emplace_back();

    for (unsigned int i = 0;
         i < nPubKeys && nSigsHave < nSigsRequired;
         ++i) {
        const valtype& pubkey = vSolutions[i + 1];
        const auto it = signatures.find(pubkey);

        if (it != signatures.end()) {
            result.emplace_back(it->second);
            ++nSigsHave;
        }
    }

    for (unsigned int i = nSigsHave;
         i < nSigsRequired;
         ++i) {
        result.emplace_back();
    }

    return result;
}

namespace
{
struct Stacks
{
    std::vector<valtype> script;

    Stacks()
    {
    }

    explicit Stacks(
        const std::vector<valtype>& scriptSigStack) :
        script(scriptSigStack)
    {
    }

    explicit Stacks(
        const SignatureData& data,
        SigVersion sigversion)
    {
        EvalScript(
            script,
            data.scriptSig,
            SCRIPT_VERIFY_STRICTENC,
            BaseSignatureChecker(),
            sigversion);
    }

    SignatureData Output() const
    {
        SignatureData result;
        result.scriptSig = PushAll(script);
        return result;
    }
};
}

static Stacks CombineSignatures(const CScript& scriptPubKey, const BaseSignatureChecker& checker, txnouttype txType, const std::vector<valtype>& vSolutions, Stacks sigs1, Stacks sigs2, SigVersion sigversion)
{
    switch (txType) {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
    case TX_ZEROCOINMINT:
        if (sigs1.script.size() >= sigs2.script.size())
            return sigs1;

        return sigs2;

    case TX_PUBKEY:
    case TX_PUBKEYHASH:
    case TX_COLDSTAKE:
        if (sigs1.script.empty() ||
            sigs1.script[0].empty()) {
            return sigs2;
        }

        return sigs1;

    case TX_SCRIPTHASH:
        if (sigs1.script.empty() ||
            sigs1.script.back().empty()) {
            return sigs2;
        }

        if (sigs2.script.empty() ||
            sigs2.script.back().empty()) {
            return sigs1;
        }

        {
            const valtype serializedScript =
                sigs1.script.back();

            const CScript redeemScript(
                serializedScript.begin(),
                serializedScript.end());

            txnouttype redeemType;
            std::vector<valtype> redeemSolutions;
            Solver(
                redeemScript,
                redeemType,
                redeemSolutions);

            sigs1.script.pop_back();
            sigs2.script.pop_back();

            Stacks result = CombineSignatures(
                redeemScript,
                checker,
                redeemType,
                redeemSolutions,
                sigs1,
                sigs2,
                sigversion);

            result.script.emplace_back(serializedScript);
            return result;
        }

    case TX_MULTISIG:
        return Stacks(
            CombineMultisig(
                scriptPubKey,
                checker,
                vSolutions,
                sigs1.script,
                sigs2.script,
                sigversion));
    }

    return Stacks();
}

SignatureData CombineSignatures(const CScript& scriptPubKey, const BaseSignatureChecker& checker, const SignatureData& scriptSig1, const SignatureData& scriptSig2)
{
    txnouttype txType;
    std::vector<valtype> vSolutions;
    Solver(scriptPubKey, txType, vSolutions);

    return CombineSignatures(
        scriptPubKey,
        checker,
        txType,
        vSolutions,
        Stacks(scriptSig1, SIGVERSION_BASE),
        Stacks(scriptSig2, SIGVERSION_BASE),
        SIGVERSION_BASE).Output();
}

namespace
{
class DummySignatureChecker : public BaseSignatureChecker
{
public:
    bool CheckSig(
        const std::vector<unsigned char>&,
        const std::vector<unsigned char>&,
        const CScript&,
        SigVersion) const override
    {
        return true;
    }
};

const DummySignatureChecker dummyChecker;
}

const BaseSignatureChecker&
DummySignatureCreator::Checker() const
{
    return dummyChecker;
}

bool DummySignatureCreator::CreateSig(std::vector<unsigned char>& vchSig, const CKeyID&, const CScript&, SigVersion) const
{
    // Create a dummy signature with valid DER encoding.
    vchSig.assign(72, '\000');
    vchSig[0] = 0x30;
    vchSig[1] = 69;
    vchSig[2] = 0x02;
    vchSig[3] = 33;
    vchSig[4] = 0x01;
    vchSig[4 + 33] = 0x02;
    vchSig[5 + 33] = 32;
    vchSig[6 + 33] = 0x01;
    vchSig[6 + 33 + 32] = SIGHASH_ALL;

    return true;
}