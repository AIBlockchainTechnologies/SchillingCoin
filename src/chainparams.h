// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CHAINPARAMS_H
#define SCHILLINGCOIN_CHAINPARAMS_H

#include "chainparamsbase.h"
#include "checkpoints.h"
#include "consensus/params.h"
#include "netaddress.h"
#include "netbase.h"
#include "primitives/block.h"
#include "protocol.h"
#include "uint256.h"

#include <memory>
#include <optional>
#include <vector>

class ArgsManager;

class CDNSSeedData {
public:
    std::string name, host;
    bool supportsServiceBitsFiltering;
    std::string getHost(uint64_t requiredServiceBits) const;
    CDNSSeedData(const std::string& strName, const std::string& strHost, bool supportsServiceBitsFilteringIn = false) : name(strName), host(strHost), supportsServiceBitsFiltering(supportsServiceBitsFilteringIn) {}
};

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * SchillingCoin system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,     // BIP16
        EXT_PUBLIC_KEY, // BIP32
        EXT_SECRET_KEY, // BIP32
        STAKING_ADDRESS,

        MAX_BASE58_TYPES
    };

    const Consensus::Params& GetConsensus() const { return consensus; }
    [[nodiscard]] std::optional<Consensus::LLMQParams> GetLLMQ(Consensus::LLMQType llmqType) const;
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }

    uint16_t GetDefaultPort() const { return nDefaultPort; }

    uint16_t GetDefaultPort(Network net) const
    {
        return net == NET_I2P ? I2P_SAM31_PORT : GetDefaultPort();
    }

    uint16_t GetDefaultPort(const std::string& addr) const
    {
        CNetAddr net_addr;
        return net_addr.SetSpecial(addr) ? GetDefaultPort(net_addr.GetNetwork()) : GetDefaultPort();
    }

    /** Allow multiple outbound addresses from the same network group. */
    bool AllowMultipleAddressesFromGroup() const
    {
        return fAllowMultipleAddressesFromGroup;
    }

    /** How long to wait before retrying an LLMQ connection. */
    int LLMQConnectionRetryTimeout() const
    {
        return nLLMQConnectionRetryTimeout;
    }

    int FulfilledRequestExpireTime() const
    {
        return nFulfilledRequestExpireTime;
    }

    const CBlock& GenesisBlock() const { return genesis; }

    /** Make miner wait to have peers to avoid wasting work */
    bool MiningRequiresPeers() const { return !IsRegTestNet(); }
    /** Headers first syncing is disabled */
    bool HeadersFirstSyncingActive() const { return false; };
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return IsRegTestNet(); }

    /** Return the network string (main, test or regtest). */
    std::string NetworkIDString() const { return ChainTypeToString(m_chain_type); }
    ChainType GetChainType() const { return m_chain_type; }
    bool IsRegTestNet() const { return m_chain_type == ChainType::REGTEST; }
    const std::vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    int ExtCoinType() const { return nExtCoinType; }
    const std::vector<CAddress>& FixedSeeds() const { return vFixedSeeds; }
    virtual const Checkpoints::CCheckpointData& Checkpoints() const = 0;

protected:
    CChainParams() = default;

    void AddLLMQ(Consensus::LLMQType llmqType);

    ChainType m_chain_type;
    CBlock genesis;
    Consensus::Params consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    uint16_t nDefaultPort;
    std::vector<CDNSSeedData> vSeeds;
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    int nExtCoinType;
    std::vector<CAddress> vFixedSeeds;
    bool fAllowMultipleAddressesFromGroup{false};
    int nLLMQConnectionRetryTimeout{60};
};

/**
 * Creates and returns parameters for the chosen chain type.
 */
std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, ChainType chain);

/**
 * Return the currently selected parameters. This won't change after app startup
 * outside of the unit tests.
 */
const CChainParams& Params();

/** Sets the params returned by Params() to those for the given chain type. */
void SelectParams(ChainType chain);

#endif // SCHILLINGCOIN_CHAINPARAMS_H