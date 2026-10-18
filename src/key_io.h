// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2021 The DECENOMY Core Developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_KEY_IO_H
#define SCHILLINGCOIN_KEY_IO_H

#include "chainparams.h"
#include "key.h"
#include "optional.h"
#include "pubkey.h"
#include "script/standard.h"

#include <string>

std::string EncodeDestination(const CTxDestination& dest, bool isStaking);
std::string EncodeDestination(const CTxDestination& dest, const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS);

// The isStaking flag is set to true when the string is a staking address.
CTxDestination DecodeDestination(const std::string& str, bool& isStaking);
CTxDestination DecodeDestination(const std::string& str);

// Return true if the string contains a valid destination.
bool IsValidDestinationString(const std::string& str);
bool IsValidDestinationString(const std::string& str, const CChainParams& params);

// Return true if the string contains a valid destination of the requested staking type.
bool IsValidDestinationString(const std::string& str, bool fStaking);
bool IsValidDestinationString(const std::string& str, bool fStaking, const CChainParams& params);

namespace KeyIO {

    CKey DecodeSecret(const std::string &str);

    std::string EncodeSecret(const CKey &key);

    CExtKey DecodeExtKey(const std::string &str);

    std::string EncodeExtKey(const CExtKey &extkey);

    CExtPubKey DecodeExtPubKey(const std::string &str);

    std::string EncodeExtPubKey(const CExtPubKey &extkey);
}

#endif // SCHILLINGCOIN_KEY_IO_H