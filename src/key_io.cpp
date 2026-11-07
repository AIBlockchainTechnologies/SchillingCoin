// Copyright (c) 2014-2019 The Bitcoin Core developers
// Copyright (c) 2021 The DECENOMY Core Developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key_io.h"

#include "base58.h"

#include <algorithm>
#include <assert.h>
#include <string.h>
#include <variant>

namespace
{

class DestinationEncoder
{
private:
    const CChainParams& m_params;
    const CChainParams::Base58Type m_addrType;

public:
    explicit DestinationEncoder(
        const CChainParams& params,
        const CChainParams::Base58Type addrType = CChainParams::PUBKEY_ADDRESS)
        : m_params(params),
          m_addrType(addrType)
    {
    }

    std::string operator()(const PKHash& id) const
    {
        std::vector<unsigned char> data = m_params.Base58Prefix(m_addrType);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const ScriptHash& id) const
    {
        std::vector<unsigned char> data = m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CNoDestination&) const
    {
        return "";
    }
};

CTxDestination DecodeDestination(
    const std::string& str,
    const CChainParams& params,
    bool& isStaking)
{
    isStaking = false;

    std::vector<unsigned char> data;
    uint160 hash;

    if (DecodeBase58Check(str, data)) {
        const std::vector<unsigned char>& pubkeyPrefix =
            params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);

        if (data.size() == hash.size() + pubkeyPrefix.size() &&
            std::equal(pubkeyPrefix.begin(), pubkeyPrefix.end(), data.begin())) {
            std::copy(data.begin() + pubkeyPrefix.size(), data.end(), hash.begin());
            return PKHash(hash);
        }

        const std::vector<unsigned char>& stakingPrefix =
            params.Base58Prefix(CChainParams::STAKING_ADDRESS);

        if (data.size() == hash.size() + stakingPrefix.size() &&
            std::equal(stakingPrefix.begin(), stakingPrefix.end(), data.begin())) {
            isStaking = true;
            std::copy(data.begin() + stakingPrefix.size(), data.end(), hash.begin());
            return PKHash(hash);
        }

        const std::vector<unsigned char>& scriptPrefix =
            params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);

        if (data.size() == hash.size() + scriptPrefix.size() &&
            std::equal(scriptPrefix.begin(), scriptPrefix.end(), data.begin())) {
            std::copy(data.begin() + scriptPrefix.size(), data.end(), hash.begin());
            return ScriptHash(hash);
        }
    }

    return CNoDestination();
}

} // namespace

std::string EncodeDestination(const CTxDestination& dest, bool isStaking)
{
    return EncodeDestination(
        dest,
        isStaking ? CChainParams::STAKING_ADDRESS : CChainParams::PUBKEY_ADDRESS);
}

std::string EncodeDestination(
    const CTxDestination& dest,
    const CChainParams::Base58Type addrType)
{
    return std::visit(DestinationEncoder(Params(), addrType), dest);
}

CTxDestination DecodeDestination(const std::string& str)
{
    bool isStaking = false;
    return DecodeDestination(str, Params(), isStaking);
}

CTxDestination DecodeDestination(const std::string& str, bool& isStaking)
{
    return DecodeDestination(str, Params(), isStaking);
}

bool IsValidDestinationString(const std::string& str)
{
    bool isStaking = false;
    return IsValidDestination(DecodeDestination(str, Params(), isStaking));
}

bool IsValidDestinationString(
    const std::string& str,
    const CChainParams& params)
{
    bool isStaking = false;
    return IsValidDestination(DecodeDestination(str, params, isStaking));
}

bool IsValidDestinationString(const std::string& str, bool fStaking)
{
    return IsValidDestinationString(str, fStaking, Params());
}

bool IsValidDestinationString(
    const std::string& str,
    bool fStaking,
    const CChainParams& params)
{
    bool isStaking = false;
    return IsValidDestination(DecodeDestination(str, params, isStaking)) &&
           isStaking == fStaking;
}

namespace KeyIO {

    CKey DecodeSecret(const std::string &str) {
        CKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data)) {
            const std::vector<unsigned char> &privkey_prefix = Params().Base58Prefix(CChainParams::SECRET_KEY);
            if ((data.size() == 32 + privkey_prefix.size() ||
                 (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
                std::equal(privkey_prefix.begin(), privkey_prefix.end(), data.begin())) {
                bool compressed = data.size() == 33 + privkey_prefix.size();
                key.Set(data.begin() + privkey_prefix.size(), data.begin() + privkey_prefix.size() + 32, compressed);
            }
        }
        if (!data.empty()) {
            memory_cleanse(data.data(), data.size());
        }
        return key;
    }

    std::string EncodeSecret(const CKey &key) {
        assert(key.IsValid());
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::SECRET_KEY);
        data.insert(data.end(), key.begin(), key.end());
        if (key.IsCompressed()) {
            data.push_back(1);
        }
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    CExtKey DecodeExtKey(const std::string &str) {
        CExtKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data)) {
            const std::vector<unsigned char> &prefix = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
            if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
                std::equal(prefix.begin(), prefix.end(), data.begin())) {
                key.Decode(data.data() + prefix.size());
            }
        }
        return key;
    }

    std::string EncodeExtKey(const CExtKey &key) {
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
        size_t size = data.size();
        data.resize(size + BIP32_EXTKEY_SIZE);
        key.Encode(data.data() + size);
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    CExtPubKey DecodeExtPubKey(const std::string &str) {
        CExtPubKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data)) {
            const std::vector<unsigned char> &prefix = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
            if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
                std::equal(prefix.begin(), prefix.end(), data.begin())) {
                key.Decode(data.data() + prefix.size());
            }
        }
        return key;
    }

    std::string EncodeExtPubKey(const CExtPubKey &key) {
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
        size_t size = data.size();
        data.resize(size + BIP32_EXTKEY_SIZE);
        key.Encode(data.data() + size);
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

} // namespace KeyIO