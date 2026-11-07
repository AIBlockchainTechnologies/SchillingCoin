// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __cplusplus
#error This header can only be compiled as C++.
#endif

#ifndef SCHILLINGCOIN_PROTOCOL_H
#define SCHILLINGCOIN_PROTOCOL_H

#include "netaddress.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "util/time.h"
#include "version.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

/** Message header.
 * (4) message start.
 * (12) command.
 * (4) size.
 * (4) checksum.
 */
class CMessageHeader
{
public:
    static constexpr size_t MESSAGE_START_SIZE = 4;
    static constexpr size_t COMMAND_SIZE = 12;
    static constexpr size_t MESSAGE_SIZE_SIZE = 4;
    static constexpr size_t CHECKSUM_SIZE = 4;
    static constexpr size_t MESSAGE_SIZE_OFFSET = MESSAGE_START_SIZE + COMMAND_SIZE;
    static constexpr size_t CHECKSUM_OFFSET = MESSAGE_SIZE_OFFSET + MESSAGE_SIZE_SIZE;
    static constexpr size_t HEADER_SIZE = MESSAGE_START_SIZE + COMMAND_SIZE + MESSAGE_SIZE_SIZE + CHECKSUM_SIZE;

    typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

    explicit CMessageHeader() = default;

    /** Construct a P2P message header from message-start characters, a command and the size of the message.
     * @note Passing in a `pszCommand` longer than COMMAND_SIZE will result in a run-time assertion error.
     */
    CMessageHeader(const MessageStartChars& pchMessageStartIn, const char* pszCommand, unsigned int nMessageSizeIn);

    std::string GetCommand() const;
    bool IsCommandValid() const;

    SERIALIZE_METHODS(CMessageHeader, obj)
    {
        READWRITE(obj.pchMessageStart, obj.pchCommand, obj.nMessageSize, obj.pchChecksum);
    }

    char pchMessageStart[MESSAGE_START_SIZE]{};
    char pchCommand[COMMAND_SIZE]{};
    uint32_t nMessageSize{std::numeric_limits<uint32_t>::max()};
    uint8_t pchChecksum[CHECKSUM_SIZE]{};
};

/**
 * P2P protocol message types.
 *
 * When adding a new message type, update the complete message-type table
 * in protocol.cpp as well.
 */
namespace NetMsgType {

extern const char* VERSION;
extern const char* VERACK;
extern const char* ADDR;
extern const char* ADDRV2;
extern const char* SENDADDRV2;
extern const char* INV;
extern const char* GETDATA;
extern const char* MERKLEBLOCK;
extern const char* GETBLOCKS;
extern const char* GETHEADERS;
extern const char* TX;
extern const char* HEADERS;
extern const char* BLOCK;
extern const char* GETADDR;
extern const char* MEMPOOL;
extern const char* PING;
extern const char* PONG;
extern const char* NOTFOUND;
extern const char* FILTERLOAD;
extern const char* FILTERADD;
extern const char* FILTERCLEAR;
extern const char* SENDHEADERS;
extern const char* SENDCMPCT;
extern const char* CMPCTBLOCK;
extern const char* GETBLOCKTXN;
extern const char* BLOCKTXN;
extern const char* GETCFILTERS;
extern const char* CFILTER;
extern const char* GETCFHEADERS;
extern const char* CFHEADERS;
extern const char* GETCFCHECKPT;
extern const char* CFCHECKPT;
extern const char* SENDTXRCNCL;

/*
 * Temporarily compiled legacy/excluded message owners.
 * Remove these declarations when their remaining callers are retired.
 */
extern const char* SPORK;
extern const char* GETSPORKS;
extern const char* DSACCEPT;
extern const char* DSVIN;
extern const char* DSFINALTX;
extern const char* DSSIGNFINALTX;
extern const char* DSCOMPLETE;
extern const char* DSSTATUSUPDATE;
extern const char* DSTX;
extern const char* DSQUEUE;
extern const char* SENDDSQUEUE;
extern const char* SYNCSTATUSCOUNT;
extern const char* MNGOVERNANCESYNC;
extern const char* MNGOVERNANCEOBJECT;
extern const char* MNGOVERNANCEOBJECTVOTE;

/* Deterministic masternode, LLMQ, ChainLocks, and InstantSend messages. */
extern const char* GETMNLISTDIFF;
extern const char* MNLISTDIFF;
extern const char* QSENDRECSIGS;
extern const char* QFCOMMITMENT;
extern const char* QCONTRIB;
extern const char* QCOMPLAINT;
extern const char* QJUSTIFICATION;
extern const char* QPCOMMITMENT;
extern const char* QWATCH;
extern const char* QSIGSESANN;
extern const char* QSIGSHARESINV;
extern const char* QGETSIGSHARES;
extern const char* QBSIGSHARES;
extern const char* QSIGREC;
extern const char* QSIGSHARE;
extern const char* QGETDATA;
extern const char* QDATA;
extern const char* CLSIG;
extern const char* ISDLOCK;
extern const char* MNAUTH;
extern const char* GETHEADERS2;
extern const char* SENDHEADERS2;
extern const char* HEADERS2;
extern const char* GETQUORUMROTATIONINFO;
extern const char* QUORUMROTATIONINFO;

/* Temporary until the remaining Platform message owner is retired. */
extern const char* PLATFORMBAN;

} // namespace NetMsgType

/** Get a vector of all valid message types. */
const std::vector<std::string>& getAllNetMessageTypes();

/** Whether the message type violates blocks-relay-only policy. */
bool NetMessageViolatesBlocksOnly(const std::string& msg_type);

/** nServices flags */
enum ServiceFlags : uint64_t {
    // NOTE: When adding here, be sure to update serviceFlagToStr too.

    NODE_NONE = 0,

    // Node can serve the complete block chain.
    NODE_NETWORK = (1 << 0),

    // Node supports bloom-filtered connections.
    NODE_BLOOM = (1 << 2),

    // Historical SCH service bit: bloom support without masternode messages.
    NODE_BLOOM_WITHOUT_MN = (1 << 4),

    // Node services BIP157/BIP158 compact block-filter requests.
    NODE_COMPACT_FILTERS = (1 << 6),

    // Node serves a limited recent portion of the chain.
    NODE_NETWORK_LIMITED = (1 << 10),

    // Node supports compressed block headers.
    NODE_HEADERS_COMPRESSED = (1 << 11),

    // Node supports BIP324 transport.
    NODE_P2P_V2 = (1 << 12),
};

/**
 * Convert service flags to human-readable strings.
 */
std::vector<std::string> serviceFlagsToStr(uint64_t flags);

/**
 * Get the set of service flags desirable for a peer.
 */
ServiceFlags GetDesirableServiceFlags(ServiceFlags services);

/** Set current IBD status for desirable-service computation. */
void SetServiceFlagsIBDCache(bool status);

/**
 * Determine whether the supplied services provide all desirable services.
 */
static inline bool HasAllDesirableServiceFlags(ServiceFlags services)
{
    return !(GetDesirableServiceFlags(services) & (~services));
}

/**
 * Determine whether a peer may have a useful address database.
 */
static inline bool MayHaveUsefulAddressDB(ServiceFlags services)
{
    return (services & NODE_NETWORK) || (services & NODE_NETWORK_LIMITED);
}

/** A CService with information about it as peer */
class CAddress : public CService
{
    static constexpr std::chrono::seconds TIME_INIT{100000000};

    /**
     * Historically, CAddress disk serialization stored the CLIENT_VERSION, optionally OR'ed with
     * the ADDRV2_FORMAT flag to indicate V2 serialization. The first field has since been
     * disentangled from client versioning, and now instead:
     * - The low bits (masked by DISK_VERSION_IGNORE_MASK) store the fixed value DISK_VERSION_INIT,
     *   (in case any code exists that treats it as a client version) but are ignored on
     *   deserialization.
     * - The high bits (masked by ~DISK_VERSION_IGNORE_MASK) store actual serialization information.
     *   Only 0 or DISK_VERSION_ADDRV2 (equal to the historical ADDRV2_FORMAT) are valid now, and
     *   any other value triggers a deserialization failure. Other values can be added later if
     *   needed.
     *
     * For disk deserialization, ADDRV2_FORMAT in the stream version signals that ADDRV2
     * deserialization is permitted, but the actual format is determined by the high bits in the
     * stored version field. For network serialization, the stream version having ADDRV2_FORMAT or
     * not determines the actual format used (as it has no embedded version number).
     */
    static constexpr uint32_t DISK_VERSION_INIT{220000};
    static constexpr uint32_t DISK_VERSION_IGNORE_MASK{0b00000000'00000111'11111111'11111111};

    /**
     * The version number written in disk serialized addresses to indicate V2 serializations.
     * It must be exactly 1<<29, as that is the value that historical versions used for this
     * (they used their internal ADDRV2_FORMAT flag here).
     */
    static constexpr uint32_t DISK_VERSION_ADDRV2{1 << 29};

    static_assert((DISK_VERSION_INIT & ~DISK_VERSION_IGNORE_MASK) == 0, "DISK_VERSION_INIT must be covered by DISK_VERSION_IGNORE_MASK");
    static_assert((DISK_VERSION_ADDRV2 & DISK_VERSION_IGNORE_MASK) == 0, "DISK_VERSION_ADDRV2 must not be covered by DISK_VERSION_IGNORE_MASK");

public:
    CAddress() : CService{} {};
    explicit CAddress(CService ipIn, ServiceFlags nServicesIn) : CService{ipIn}, nServices{nServicesIn} {};
    CAddress(CService ipIn, ServiceFlags nServicesIn, NodeSeconds time) : CService{ipIn}, nTime{time}, nServices{nServicesIn} {};

    SERIALIZE_METHODS(CAddress, obj)
    {
        // CAddress has a distinct network serialization and a disk serialization, but it should never
        // be hashed (except through CHashWriter in addrdb.cpp, which sets SER_DISK), and it's
        // ambiguous what that would mean. Make sure no code relying on that is introduced:
        assert(!(s.GetType() & SER_GETHASH));

        bool use_v2;

        if (s.GetType() & SER_DISK) {
            // In the disk serialization format, the encoding (v1 or v2) is determined by a flag version
            // that's part of the serialization itself. ADDRV2_FORMAT in the stream version only determines
            // whether V2 is chosen/permitted at all.
            uint32_t stored_format_version = DISK_VERSION_INIT;

            if (s.GetVersion() & ADDRV2_FORMAT) stored_format_version |= DISK_VERSION_ADDRV2;

            READWRITE(stored_format_version);

            stored_format_version &= ~DISK_VERSION_IGNORE_MASK; // ignore low bits

            if (stored_format_version == 0) {
                use_v2 = false;
            } else if (stored_format_version == DISK_VERSION_ADDRV2 && (s.GetVersion() & ADDRV2_FORMAT)) {
                // Only support v2 deserialization if ADDRV2_FORMAT is set.
                use_v2 = true;
            } else {
                throw std::ios_base::failure("Unsupported CAddress disk format version");
            }
        } else {
            // In the network serialization format, the encoding (v1 or v2) is determined directly by
            // the value of ADDRV2_FORMAT in the stream version, as no explicitly encoded version
            // exists in the stream.
            assert(s.GetType() & SER_NETWORK);
            use_v2 = s.GetVersion() & ADDRV2_FORMAT;
        }

        READWRITE(Using<LossyChronoFormatter<uint32_t>>(obj.nTime));

        // nServices is serialized as CompactSize in V2; as uint64_t in V1.
        if (use_v2) {
            uint64_t services_tmp;
            SER_WRITE(obj, services_tmp = obj.nServices);
            READWRITE(Using<CompactSizeFormatter<false>>(services_tmp));
            SER_READ(obj, obj.nServices = static_cast<ServiceFlags>(services_tmp));
        } else {
            READWRITE(Using<CustomUintFormatter<8>>(obj.nServices));
        }

        // Invoke V1/V2 serializer for CService parent object.
        OverrideStream<Stream> os(&s, s.GetType(), use_v2 ? ADDRV2_FORMAT : 0);
        SerReadWriteMany(os, ser_action, ReadWriteAsHelper<CService>(obj));
    }

    //! Always included in serialization. The behavior is unspecified if the value is not representable as uint32_t.
    NodeSeconds nTime{TIME_INIT};

    //! Serialized as uint64_t in V1, and as CompactSize in V2.
    ServiceFlags nServices{NODE_NONE};

    friend bool operator==(const CAddress& a, const CAddress& b)
    {
        return a.nTime == b.nTime &&
               a.nServices == b.nServices &&
               static_cast<const CService&>(a) == static_cast<const CService&>(b);
    }
};

/** getdata / inv message types. */
enum GetDataMsg : uint32_t {
    UNDEFINED = 0,
    MSG_TX = 1,
    MSG_BLOCK = 2,

    // The following can only occur in getdata. Invs always use TX or BLOCK.
    MSG_FILTERED_BLOCK = 3,

    // Historical SCH inventory namespace. These numeric values must not change.
    // Values 4 and 5 were historical InstantSend inventory types.
    // Value 6 was the historical spork inventory type.
    MSG_MASTERNODE_WINNER = 7,
    // Values 8 through 13 were historical SCH masternode/budget inventory types.
    MSG_MASTERNODE_ANNOUNCE = 14,
    MSG_MASTERNODE_PING = 15,
    MSG_DSTX = 16,

    // Temporarily retained while the remaining governance owners are retired.
    MSG_GOVERNANCE_OBJECT = 17,
    MSG_GOVERNANCE_OBJECT_VOTE = 18,

    // Value 19 remains reserved.

    // Modern donor-aligned inventory namespace.
    MSG_CMPCT_BLOCK = 20,
    MSG_QUORUM_FINAL_COMMITMENT = 21,
    // Value 22 was historically used for a quorum dummy commitment.
    MSG_QUORUM_CONTRIB = 23,
    MSG_QUORUM_COMPLAINT = 24,
    MSG_QUORUM_JUSTIFICATION = 25,
    MSG_QUORUM_PREMATURE_COMMITMENT = 26,
    // Value 27 was historically used for quorum debug status.
    MSG_QUORUM_RECOVERED_SIG = 28,
    MSG_CLSIG = 29,
    // Value 30 was used for legacy non-deterministic InstantSend locks.
    MSG_ISDLOCK = 31,

    // Temporary compiled owners to be removed with their excluded subsystems.
    MSG_DSQ = 32,
    MSG_PLATFORM_BAN = 33,
    MSG_ASSET_UNLOCK = 34,
};

/** inv message data */
class CInv
{
public:
    CInv();
    CInv(uint32_t typeIn, const uint256& hashIn);

    SERIALIZE_METHODS(CInv, obj)
    {
        READWRITE(obj.type, obj.hash);
    }

    friend bool operator<(const CInv& a, const CInv& b);
    friend bool operator==(const CInv& a, const CInv& b)
    {
        return a.type == b.type && a.hash == b.hash;
    }

    bool IsKnownType() const;
    bool IsMasterNodeType() const;
    std::string GetCommand() const;
    std::string ToString() const;

    bool IsMsgTx() const { return type == MSG_TX; }
    bool IsMsgBlk() const { return type == MSG_BLOCK; }
    bool IsMsgDstx() const { return type == MSG_DSTX; }
    bool IsMsgAssetUnlock() const { return type == MSG_ASSET_UNLOCK; }
    bool IsMsgFilteredBlk() const { return type == MSG_FILTERED_BLOCK; }
    bool IsMsgCmpctBlk() const { return type == MSG_CMPCT_BLOCK; }

    bool IsGenTxMsg() const
    {
        return type == MSG_TX || type == MSG_DSTX || type == MSG_ASSET_UNLOCK;
    }

    bool IsGenBlkMsg() const
    {
        return type == MSG_BLOCK || type == MSG_FILTERED_BLOCK || type == MSG_CMPCT_BLOCK;
    }

private:
    const char* GetCommandInternal() const;

public:
    uint32_t type;
    uint256 hash;
};

#endif // SCHILLINGCOIN_PROTOCOL_H