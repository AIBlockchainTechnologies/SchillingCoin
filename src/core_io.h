// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CORE_IO_H
#define SCHILLINGCOIN_CORE_IO_H

#include <string>
#include <vector>

class CBlock;
class CScript;
class CTransaction;
class CTxUndo;
class CSpentIndexTxInfo;
class uint256;
class UniValue;

enum class TxVerbosity {
    SHOW_TXID,
    SHOW_DETAILS,
    SHOW_DETAILS_AND_PREVOUT,
};

// core_read.cpp
extern CScript ParseScript(std::string s);
extern std::string ScriptToAsmStr(const CScript& script, const bool fAttemptSighashDecode = false);
extern bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);
extern bool DecodeHexBlk(CBlock&, const std::string& strHexBlk);
extern uint256 ParseHashUV(const UniValue& v, const std::string& strName);
extern uint256 ParseHashStr(const std::string&, const std::string& strName);
extern std::vector<unsigned char> ParseHexUV(const UniValue& v, const std::string& strName);

// core_write.cpp
extern UniValue ValueFromAmount(const CAmount amount);
extern std::string FormatScript(const CScript& script);
extern std::string EncodeHexTx(const CTransaction& tx);
extern std::string SighashToStr(unsigned char sighash_type);
extern void ScriptToUniv(const CScript& script, UniValue& out, bool include_hex = true, bool include_address = false);
extern void TxToUniv(const CTransaction& tx, const uint256& block_hash, UniValue& entry, bool include_hex = true, int serialize_flags = 0, const CTxUndo* txundo = nullptr, TxVerbosity verbosity = TxVerbosity::SHOW_DETAILS, const CSpentIndexTxInfo* ptxSpentInfo = nullptr);

#endif // SCHILLINGCOIN_CORE_IO_H