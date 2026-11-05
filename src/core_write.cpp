// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core_io.h"

#include "coins.h"
#include "consensus/amount.h"
#include "evo/cbtx.h"
#include "evo/mnhftx.h"
#include "evo/providertx.h"
#include "evo/specialtx.h"
#include "index/spentindex_types.h"
#include "key_io.h"
#include "llmq/commitment.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/standard.h"
#include "serialize.h"
#include "streams.h"
#include "undo.h"
#include "util/check.h"
#include "util/system.h"
#include "utilstrencodings.h"

#include <univalue.h>

#include <map>
#include <string>
#include <vector>

UniValue ValueFromAmount(const CAmount amount)
{
    static_assert(COIN > 1);

    int64_t quotient = amount / COIN;
    int64_t remainder = amount % COIN;

    if (amount < 0) {
        quotient = -quotient;
        remainder = -remainder;
    }

    return UniValue(
        UniValue::VNUM,
        strprintf("%s%d.%08d", amount < 0 ? "-" : "", quotient, remainder));
}

std::string FormatScript(const CScript& script)
{
    std::string ret;
    CScript::const_iterator it = script.begin();
    opcodetype op;
    while (it != script.end()) {
        CScript::const_iterator it2 = it;
        std::vector<unsigned char> vch;
        if (script.GetOp2(it, op, &vch)) {
            if (op == OP_0) {
                ret += "0 ";
                continue;
            } else if ((op >= OP_1 && op <= OP_16) || op == OP_1NEGATE) {
                ret += strprintf("%i ", op - OP_1NEGATE - 1);
                continue;
            } else if (op >= OP_NOP && op <= OP_CHECKMULTISIGVERIFY) {
                std::string str(GetOpName(op));
                if (str.substr(0, 3) == std::string("OP_")) {
                    ret += str.substr(3, std::string::npos) + " ";
                    continue;
                }
            }
            if (vch.size() > 0) {
                ret += strprintf("0x%x 0x%x ", HexStr(it2, it - vch.size()), HexStr(it - vch.size(), it));
            } else {
                ret += strprintf("0x%x", HexStr(it2, it));
            }
            continue;
        }
        ret += strprintf("0x%x ", HexStr(it2, script.end()));
        break;
    }
    return ret.substr(0, ret.size() - 1);
}

const std::map<unsigned char, std::string> mapSigHashTypes = {
    {static_cast<unsigned char>(SIGHASH_ALL), std::string("ALL")},
    {static_cast<unsigned char>(SIGHASH_ALL | SIGHASH_ANYONECANPAY), std::string("ALL|ANYONECANPAY")},
    {static_cast<unsigned char>(SIGHASH_NONE), std::string("NONE")},
    {static_cast<unsigned char>(SIGHASH_NONE | SIGHASH_ANYONECANPAY), std::string("NONE|ANYONECANPAY")},
    {static_cast<unsigned char>(SIGHASH_SINGLE), std::string("SINGLE")},
    {static_cast<unsigned char>(SIGHASH_SINGLE | SIGHASH_ANYONECANPAY), std::string("SINGLE|ANYONECANPAY")},
};

std::string SighashToStr(unsigned char sighash_type)
{
    const auto& it = mapSigHashTypes.find(sighash_type);
    if (it == mapSigHashTypes.end()) return "";
    return it->second;
}

/**
 * Create the assembly string representation of a CScript object.
 * @param[in] script    CScript object to convert into the asm string representation.
 * @param[in] fAttemptSighashDecode    Whether to attempt to decode sighash types on data within the script that matches the format
 *                                     of a signature. Only pass true for scripts you believe could contain signatures. For example,
 *                                     pass false, or omit the this argument (defaults to false), for scriptPubKeys.
 */
std::string ScriptToAsmStr(const CScript& script, const bool fAttemptSighashDecode)
{
    std::string str;
    opcodetype opcode;
    std::vector<unsigned char> vch;
    CScript::const_iterator pc = script.begin();
    while (pc < script.end()) {
        if (!str.empty()) {
            str += " ";
        }
        if (!script.GetOp(pc, opcode, vch)) {
            str += "[error]";
            return str;
        }
        if (0 <= opcode && opcode <= OP_PUSHDATA4) {
            if (vch.size() <= static_cast<std::vector<unsigned char>::size_type>(4)) {
                str += strprintf("%d", CScriptNum(vch, false).getint());
            } else {
                // the IsUnspendable check makes sure not to try to decode OP_RETURN data that may match the format of a signature
                if (fAttemptSighashDecode && !script.IsUnspendable()) {
                    std::string strSigHashDecode;
                    // goal: only attempt to decode a defined sighash type from data that looks like a signature within a scriptSig.
                    // this won't decode correctly formatted public keys in Pubkey or Multisig scripts due to
                    // the restrictions on the pubkey formats (see IsCompressedOrUncompressedPubKey) being incongruous with the
                    // checks in CheckSignatureEncoding.
                    if (CheckSignatureEncoding(vch, SCRIPT_VERIFY_STRICTENC, NULL)) {
                        const unsigned char chSigHashType = vch.back();
                        const auto it = mapSigHashTypes.find(chSigHashType);
                        if (it != mapSigHashTypes.end()) {
                            strSigHashDecode = "[" + it->second + "]";
                            vch.pop_back(); // remove the sighash type byte. it will be replaced by the decode.
                        }
                    }
                    str += HexStr(vch) + strSigHashDecode;
                } else {
                    str += HexStr(vch);
                }
            }
        } else {
            str += GetOpName(opcode);
        }
    }
    return str;
}

std::string EncodeHexTx(const CTransaction& tx)
{
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;
    return HexStr(ssTx);
}

void ScriptToUniv(const CScript& script, UniValue& out, bool include_hex, bool include_address)
{
    txnouttype type;
    std::vector<CTxDestination> addresses;
    int nRequired;

    out.pushKV("asm", ScriptToAsmStr(script));

    if (include_hex) {
        out.pushKV("hex", HexStr(script));
    }

    if (!ExtractDestinations(script, type, addresses, nRequired)) {
        out.pushKV("type", GetTxnOutputType(type));
        return;
    }

    out.pushKV("reqSigs", nRequired);
    out.pushKV("type", GetTxnOutputType(type));

    if (!include_address) {
        return;
    }

    UniValue addresses_json(UniValue::VARR);

    if (type == TX_COLDSTAKE && addresses.size() == 2) {
        addresses_json.push_back(
            EncodeDestination(addresses[0], CChainParams::STAKING_ADDRESS));
        addresses_json.push_back(
            EncodeDestination(addresses[1], CChainParams::PUBKEY_ADDRESS));
    } else {
        for (const CTxDestination& address : addresses) {
            addresses_json.push_back(EncodeDestination(address));
        }
    }

    out.pushKV("addresses", addresses_json);
}

void TxToUniv(const CTransaction& tx, const uint256& block_hash, UniValue& entry, bool include_hex, int serialize_flags, const CTxUndo* txundo, TxVerbosity verbosity, const CSpentIndexTxInfo* ptxSpentInfo)
{
    CHECK_NONFATAL(verbosity >= TxVerbosity::SHOW_DETAILS);

    const uint256 txid = tx.GetHash();
    entry.pushKV("txid", txid.GetHex());
    entry.pushKV("version", static_cast<int64_t>(static_cast<uint16_t>(tx.nVersion)));
    entry.pushKV("type", tx.nType);
    entry.pushKV("size", (int)::GetSerializeSize(tx, PROTOCOL_VERSION));
    entry.pushKV("locktime", (int64_t)tx.nLockTime);

    UniValue vin(UniValue::VARR);

    const bool have_undo = txundo != nullptr;
    CAmount amt_total_in = 0;
    CAmount amt_total_out = 0;

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxIn& txin = tx.vin[i];
        UniValue in(UniValue::VOBJ);

        if (tx.IsCoinBase()) {
            in.pushKV("coinbase", HexStr(txin.scriptSig));
        } else {
            in.pushKV("txid", txin.prevout.hash.GetHex());
            in.pushKV("vout", (int64_t)txin.prevout.n);

            UniValue script_sig(UniValue::VOBJ);
            script_sig.pushKV("asm", ScriptToAsmStr(txin.scriptSig, true));
            script_sig.pushKV("hex", HexStr(txin.scriptSig));
            in.pushKV("scriptSig", script_sig);

            if (ptxSpentInfo != nullptr) {
                CSpentIndexKey spent_key(txin.prevout.hash, txin.prevout.n);
                const auto it = ptxSpentInfo->mSpentInfo.find(spent_key);

                if (it != ptxSpentInfo->mSpentInfo.end()) {
                    const auto& spent_info = it->second;

                    in.pushKV("value", ValueFromAmount(spent_info.m_amount));
                    in.pushKV("valueSat", spent_info.m_amount);

                    if (spent_info.m_address_type == AddressType::P2PK_OR_P2PKH) {
                        in.pushKV("address", EncodeDestination(PKHash(spent_info.m_address_bytes)));
                    } else if (spent_info.m_address_type == AddressType::P2SH) {
                        in.pushKV("address", EncodeDestination(ScriptHash(spent_info.m_address_bytes)));
                    }
                }
            }
        }

        if (have_undo) {
            const Coin& prev_coin = txundo->vprevout[i];
            const CTxOut& prev_txout = prev_coin.out;

            amt_total_in += prev_txout.nValue;

            if (verbosity == TxVerbosity::SHOW_DETAILS_AND_PREVOUT) {
                UniValue script_pub_key(UniValue::VOBJ);
                ScriptToUniv(
                    prev_txout.scriptPubKey,
                    script_pub_key,
                    /*include_hex=*/true,
                    /*include_address=*/true);

                UniValue prevout(UniValue::VOBJ);
                prevout.pushKV("generated", bool(prev_coin.fCoinBase));
                prevout.pushKV("height", uint64_t(prev_coin.nHeight));
                prevout.pushKV("value", ValueFromAmount(prev_txout.nValue));
                prevout.pushKV("scriptPubKey", script_pub_key);

                in.pushKV("prevout", prevout);
            }
        }

        in.pushKV("sequence", (int64_t)txin.nSequence);
        vin.push_back(in);
    }

    entry.pushKV("vin", vin);

    UniValue vout(UniValue::VARR);

    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];

        UniValue out(UniValue::VOBJ);
        out.pushKV("value", ValueFromAmount(txout.nValue));
        out.pushKV("valueSat", txout.nValue);
        out.pushKV("n", (int64_t)i);

        UniValue script_pub_key(UniValue::VOBJ);
        ScriptToUniv(
            txout.scriptPubKey,
            script_pub_key,
            /*include_hex=*/true,
            /*include_address=*/true);
        out.pushKV("scriptPubKey", script_pub_key);

        if (ptxSpentInfo != nullptr) {
            CSpentIndexKey spent_key(txid, i);
            const auto it = ptxSpentInfo->mSpentInfo.find(spent_key);

            if (it != ptxSpentInfo->mSpentInfo.end()) {
                const auto& spent_info = it->second;
                out.pushKV("spentTxId", spent_info.m_tx_hash.GetHex());
                out.pushKV("spentIndex", spent_info.m_tx_index);
                out.pushKV("spentHeight", spent_info.m_block_height);
            }
        }

        vout.push_back(out);

        if (have_undo) {
            amt_total_out += txout.nValue;
        }
    }

    entry.pushKV("vout", vout);

    if (!tx.vExtraPayload.empty()) {
        entry.pushKV("extraPayloadSize", tx.vExtraPayload.size());
        entry.pushKV("extraPayload", HexStr(tx.vExtraPayload));
    }

    if (tx.nType == TRANSACTION_PROVIDER_REGISTER) {
        if (const auto pro_tx = GetTxPayload<CProRegTx>(tx)) {
            entry.pushKV("proRegTx", pro_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_PROVIDER_UPDATE_SERVICE) {
        if (const auto pro_tx = GetTxPayload<CProUpServTx>(tx)) {
            entry.pushKV("proUpServTx", pro_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_PROVIDER_UPDATE_REGISTRAR) {
        if (const auto pro_tx = GetTxPayload<CProUpRegTx>(tx)) {
            entry.pushKV("proUpRegTx", pro_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_PROVIDER_UPDATE_REVOKE) {
        if (const auto pro_tx = GetTxPayload<CProUpRevTx>(tx)) {
            entry.pushKV("proUpRevTx", pro_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_COINBASE) {
        if (const auto cb_tx = GetTxPayload<CCbTx>(tx)) {
            entry.pushKV("cbTx", cb_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_QUORUM_COMMITMENT) {
        if (const auto qc_tx = GetTxPayload<llmq::CFinalCommitmentTxPayload>(tx)) {
            entry.pushKV("qcTx", qc_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_MNHF_SIGNAL) {
        if (const auto mnhf_tx = GetTxPayload<MNHFTxPayload>(tx)) {
            entry.pushKV("mnhfTx", mnhf_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_PROVIDER_DISSOLVE) {
        if (const auto pro_tx = GetTxPayload<CProDisTx>(tx)) {
            entry.pushKV("proDisTx", pro_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_PROVIDER_UPDATE_SHARE) {
        if (const auto pro_tx = GetTxPayload<CProUpShareTx>(tx)) {
            entry.pushKV("proUpShareTx", pro_tx->ToJson());
        }
    } else if (tx.nType == TRANSACTION_PROVIDER_UPDATE_SHARED_REGISTRAR) {
        if (const auto pro_tx = GetTxPayload<CProUpSharedRegTx>(tx)) {
            entry.pushKV("proUpSharedRegTx", pro_tx->ToJson());
        }
    }

    if (have_undo) {
        const CAmount fee = amt_total_in - amt_total_out;
        CHECK_NONFATAL(MoneyRange(fee));
        entry.pushKV("fee", ValueFromAmount(fee));
    }

    if (!block_hash.IsNull()) {
        entry.pushKV("blockhash", block_hash.GetHex());
    }

    if (include_hex) {
        entry.pushKV("hex", EncodeHexTx(tx));
    }
}