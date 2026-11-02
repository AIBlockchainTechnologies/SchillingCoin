// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_NET_H
#define SCHILLINGCOIN_NET_H

#include "addrdb.h"
#include "addrman.h"
#include "bloom.h"
#include "compat.h"
#include "hash.h"
#include "limitedmap.h"
#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "streams.h"
#include "sync.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include <util/threadinterrupt.h>

#include <atomic>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <stdint.h>

#ifndef WIN32
#include <arpa/inet.h>
#endif

#include <boost/signals2/signal.hpp>

class CAddrMan;
class CBlockIndex;
class CScheduler;
class CNode;

namespace boost
{
class thread_group;
} // namespace boost

/** Time between pings automatically sent out for latency probing and keepalive (in seconds). */
static const int PING_INTERVAL = 2 * 60;
/** Time after which to disconnect, after waiting for a ping response (or inactivity). */
static const int TIMEOUT_INTERVAL = 20 * 60;
/** The maximum number of entries in an 'inv' protocol message */
static const unsigned int MAX_INV_SZ = 50000;
/** The maximum number of entries in a locator */
static const unsigned int MAX_LOCATOR_SZ = 101;
/** The maximum number of new addresses to accumulate before announcing. */
static const unsigned int MAX_ADDR_TO_SEND = 1000;
/** Maximum length of incoming protocol messages (no message over 2 MiB is currently acceptable). */
static const unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 2 * 1024 * 1024;
/** Maximum length of strSubVer in `version` message */
static const unsigned int MAX_SUBVERSION_LENGTH = 256;
/** -listen default */
static const bool DEFAULT_LISTEN = true;
/** The maximum number of entries in mapAskFor */
static const size_t MAPASKFOR_MAX_SZ = MAX_INV_SZ;
/** The maximum number of peer connections to maintain. */
static const unsigned int DEFAULT_MAX_PEER_CONNECTIONS = 125;
/** Disconnected peers are added to setOffsetDisconnectedPeers only if node has less than ENOUGH_CONNECTIONS */
#define ENOUGH_CONNECTIONS 2
/** Maximum number of peers added to setOffsetDisconnectedPeers before triggering a warning */
#define MAX_TIMEOFFSET_DISCONNECTIONS 16

bool RecvLine(SOCKET hSocket, std::string& strLine);
void MapPort(bool fUseUPnP);
unsigned short GetListenPort();
void CheckOffsetDisconnectedPeers(const CNetAddr& ip);

typedef int NodeId;

class CClientUIInterface;
class CNodeStats;
class CTransaction;
class NetEventsInterface;

class CConnman
{
public:
    enum NumConnections {
        CONNECTIONS_NONE = 0,
        CONNECTIONS_IN = (1U << 0),
        CONNECTIONS_OUT = (1U << 1),
        CONNECTIONS_ALL = CONNECTIONS_IN | CONNECTIONS_OUT,
    };

    struct Options
    {
        ServiceFlags nLocalServices = NODE_NONE;
        int nMaxConnections = 0;
        int nMaxOutbound = 0;
        int nBestHeight = 0;
        CClientUIInterface* uiInterface = nullptr;
        NetEventsInterface* m_msgproc = nullptr;
        unsigned int nSendBufferMaxSize = 0;
        unsigned int nReceiveFloodSize = 0;
    };

    CConnman(uint64_t nSeed0In, uint64_t nSeed1In);
    ~CConnman();

    bool Start(
        boost::thread_group& threadGroup,
        CScheduler& scheduler,
        std::string& strNodeError,
        const Options& options);

    void Interrupt();
    void Stop();

    bool BindListenPort(
        const CService& bindAddr,
        std::string& strError,
        bool fWhitelisted = false);

    bool OpenNetworkConnection(
        const CAddress& addrConnect,
        bool fCountFailure,
        CSemaphoreGrant* grantOutbound = nullptr,
        const char* strDest = nullptr,
        bool fOneShot = false);

    CNode* ConnectNode(
        CAddress addrConnect,
        const char* pszDest = nullptr,
        bool fCountFailure = false);

    bool CheckIncomingNonce(uint64_t nonce);

    bool ForNode(
        NodeId id,
        const std::function<bool(CNode*)>& func);

    bool ForNode(
        const CService& addr,
        const std::function<bool(CNode*)>& func);

    template <typename Callable>
    bool ForEachNodeContinueIf(Callable&& func)
    {
        LOCK(cs_vNodes);
        for (CNode* node : vNodes) {
            if (!func(node))
                return false;
        }
        return true;
    }

    template <typename Callable>
    bool ForEachNodeContinueIf(Callable&& func) const
    {
        LOCK(cs_vNodes);
        for (const CNode* node : vNodes) {
            if (!func(node))
                return false;
        }
        return true;
    }

    template <typename Callable, typename CallableAfter>
    bool ForEachNodeContinueIfThen(Callable&& pre, CallableAfter&& post)
    {
        bool result = true;
        LOCK(cs_vNodes);
        for (CNode* node : vNodes) {
            if (!pre(node)) {
                result = false;
                break;
            }
        }
        post();
        return result;
    }

    template <typename Callable, typename CallableAfter>
    void ForEachNodeThen(Callable&& pre, CallableAfter&& post)
    {
        LOCK(cs_vNodes);
        for (CNode* node : vNodes)
            pre(node);
        post();
    }

    template <typename Callable>
    void ForEachNode(Callable&& func)
    {
        LOCK(cs_vNodes);
        for (CNode* node : vNodes)
            func(node);
    }

    std::vector<CNode*> CopyNodeVector();
    void ReleaseNodeVector(const std::vector<CNode*>& nodes);

    void RelayTransaction(const CTransaction& tx);
    void RelayTransaction(const CTransaction& tx, const CDataStream& ss);
    void RelayInv(CInv& inv);

    size_t GetAddressCount() const;
    void SetServices(const CService& addr, ServiceFlags services);
    void MarkAddressGood(const CAddress& addr);
    void AddNewAddress(
        const CAddress& addr,
        const CAddress& source,
        int64_t timePenalty = 0);
    void AddNewAddresses(
        const std::vector<CAddress>& addresses,
        const CAddress& source,
        int64_t timePenalty = 0);
    std::vector<CAddress> GetAddresses();
    void AddressCurrentlyConnected(const CService& addr);

    void Ban(
        const CNetAddr& addr,
        const BanReason& reason,
        int64_t banTimeOffset = 0,
        bool sinceUnixEpoch = false);
    void Ban(
        const CSubNet& subnet,
        const BanReason& reason,
        int64_t banTimeOffset = 0,
        bool sinceUnixEpoch = false);
    void ClearBanned();
    bool IsBanned(const CNetAddr& addr);
    bool IsBanned(const CSubNet& subnet);
    bool Unban(const CNetAddr& addr);
    bool Unban(const CSubNet& subnet);
    void GetBanned(banmap_t& banMap);
    void SetBanned(const banmap_t& banMap);

    void AddOneShot(const std::string& destination);

    size_t GetNodeCount(NumConnections connections) const;
    void GetNodeStats(std::vector<CNodeStats>& stats);

    bool DisconnectNode(const std::string& node);
    bool DisconnectNode(NodeId id);

    unsigned int GetReceiveFloodSize() const;
    unsigned int GetSendBufferSize() const;

    void AddWhitelistedRange(const CSubNet& subnet);

    ServiceFlags GetLocalServices() const;
    void SetBestHeight(int height);
    int GetBestHeight() const;

    uint64_t GetTotalBytesRecv() const;
    uint64_t GetTotalBytesSent() const;

    /** Get a unique deterministic randomizer. */
    CSipHasher GetDeterministicRandomizer(uint64_t id) const;

private:
    struct ListenSocket
    {
        SOCKET socket;
        bool whitelisted;

        ListenSocket(SOCKET socketIn, bool whitelistedIn)
            : socket(socketIn),
              whitelisted(whitelistedIn)
        {
        }
    };

    void ProcessOneShot();
    void ThreadDNSAddressSeed();
    void ThreadOpenAddedConnections();
    void ThreadOpenConnections();
    void ThreadMessageHandler();
    void ThreadSocketHandler();
    void SocketSendData(CNode* pnode);
    void WakeMessageHandler();

    CNode* FindNode(const CNetAddr& ip);
    CNode* FindNode(const CSubNet& subnet);
    CNode* FindNode(const std::string& addressName);
    CNode* FindNode(const CService& service);

    bool IsWhitelistedRange(const CNetAddr& addr) const;
    void DeleteNode(CNode* node);
    NodeId GetNewNodeId();

    bool BannedSetIsDirty() const;
    void SetBannedSetDirty(bool dirty = true);
    void SweepBanned();

    void DumpAddresses();
    void DumpBanlist();
    void DumpData();

    void RecordBytesRecv(uint64_t bytes);
    void RecordBytesSent(uint64_t bytes);

    RecursiveMutex cs_totalBytesRecv;
    RecursiveMutex cs_totalBytesSent;
    uint64_t nTotalBytesRecv;
    uint64_t nTotalBytesSent;

    std::vector<CSubNet> vWhitelistedRange;
    mutable RecursiveMutex cs_vWhitelistedRange;

    unsigned int nSendBufferMaxSize;
    unsigned int nReceiveFloodSize;

    std::vector<ListenSocket> vhListenSocket;

    banmap_t setBanned;
    mutable RecursiveMutex cs_setBanned;
    bool setBannedIsDirty;

    bool fAddressesInitialized;
    CAddrMan addrman;

    std::deque<std::string> vOneShots;
    RecursiveMutex cs_vOneShots;

    std::vector<std::string> vAddedNodes;
    RecursiveMutex cs_vAddedNodes;

    std::vector<CNode*> vNodes;
    std::list<CNode*> vNodesDisconnected;
    mutable RecursiveMutex cs_vNodes;

    std::atomic<NodeId> nLastNodeId;

    /** Message processor implemented by PeerManager. */
    NetEventsInterface* m_msgproc;

    /** Flag for waking the message processor. */
    bool fMsgProcWake;

    std::condition_variable condMsgProc;
    Mutex mutexMsgProc;
    std::atomic<bool> flagInterruptMsgProc{false};

    CThreadInterrupt interruptNet;

    ServiceFlags nLocalServices;
    CSemaphore* semOutbound;

    int nMaxConnections;
    int nMaxOutbound;
    std::atomic<int> nBestHeight;
    CClientUIInterface* clientInterface;

    /** SipHasher seeds for deterministic randomness */
    const uint64_t nSeed0;
    const uint64_t nSeed1;
};

extern std::unique_ptr<CConnman> g_connman;

struct CombinerAll
{
    typedef bool result_type;

    template <typename Iterator>
    bool operator()(Iterator first, Iterator last) const
    {
        while (first != last) {
            if (!*first)
                return false;
            ++first;
        }
        return true;
    }
};

// Signals for message handling
struct CNodeSignals
{
    boost::signals2::signal<int()> GetHeight;
    boost::signals2::signal<bool(CNode*), CombinerAll> ProcessMessages;
    boost::signals2::signal<bool(CNode*, bool), CombinerAll> SendMessages;
    boost::signals2::signal<void(NodeId, const CNode*)> InitializeNode;
    boost::signals2::signal<void(NodeId)> FinalizeNode;
};

CNodeSignals& GetNodeSignals();

enum {
    LOCAL_NONE,   // unknown
    LOCAL_IF,     // address a local interface listens on
    LOCAL_BIND,   // address explicit bound to
    LOCAL_UPNP,   // address reported by UPnP
    LOCAL_MANUAL, // address explicitly specified (-externalip=)

    LOCAL_MAX
};

bool IsPeerAddrLocalGood(CNode* pnode);
void AdvertiseLocal(CNode* pnode);
void SetLimited(enum Network net, bool fLimited = true);
bool IsLimited(enum Network net);
bool IsLimited(const CNetAddr& addr);
bool AddLocal(const CService& addr, int nScore = LOCAL_NONE);
bool AddLocal(const CNetAddr& addr, int nScore = LOCAL_NONE);
bool RemoveLocal(const CService& addr);
bool SeenLocal(const CService& addr);
bool IsLocal(const CService& addr);
bool GetLocal(CService& addr, const CNetAddr* paddrPeer = NULL);
bool IsReachable(enum Network net);
bool IsReachable(const CNetAddr& addr);
CAddress GetLocalAddress(const CNetAddr* paddrPeer = NULL);
bool validateMasternodeIP(const std::string& addrStr);

extern bool fDiscover;
extern bool fListen;

extern std::map<CInv, CDataStream> mapRelay;
extern std::deque<std::pair<int64_t, CInv> > vRelayExpiration;
extern RecursiveMutex cs_mapRelay;
extern limitedmap<CInv, int64_t> mapAlreadyAskedFor;

/** Subversion as sent to the P2P network in `version` messages */
extern std::string strSubVersion;

struct LocalServiceInfo {
    int nScore;
    int nPort;
};

extern RecursiveMutex cs_mapLocalHost;
extern std::map<CNetAddr, LocalServiceInfo> mapLocalHost;

class CNodeStats
{
public:
    NodeId nodeid;
    uint64_t nServices;
    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nTimeConnected;
    int64_t nTimeOffset;
    std::string addrName;
    int nVersion;
    std::string cleanSubVer;
    bool fInbound;
    int nStartingHeight;
    uint64_t nSendBytes;
    uint64_t nRecvBytes;
    bool fWhitelisted;
    double dPingTime;
    double dPingWait;
    std::string addrLocal;
};


class CNetMessage
{
public:
    bool in_data; // parsing header (false) or data (true)

    CDataStream hdrbuf; // partially received header
    CMessageHeader hdr; // complete header
    unsigned int nHdrPos;

    CDataStream vRecv; // received message data
    unsigned int nDataPos;

    int64_t nTime; // time (in microseconds) of message receipt.

    CNetMessage(int nTypeIn, int nVersionIn) : hdrbuf(nTypeIn, nVersionIn), vRecv(nTypeIn, nVersionIn)
    {
        hdrbuf.resize(24);
        in_data = false;
        nHdrPos = 0;
        nDataPos = 0;
        nTime = 0;
    }

    bool complete() const
    {
        if (!in_data)
            return false;
        return (hdr.nMessageSize == nDataPos);
    }

    void SetVersion(int nVersionIn)
    {
        hdrbuf.SetVersion(nVersionIn);
        vRecv.SetVersion(nVersionIn);
    }

    int readHeader(const char* pch, unsigned int nBytes);
    int readData(const char* pch, unsigned int nBytes);
};

/** Information about a peer */
class CNode
{
    friend class CConnman;

public:
    // socket
    ServiceFlags nServices;
    SOCKET hSocket;
    CDataStream ssSend;
    size_t nSendSize;   // total size of all vSendMsg entries
    size_t nSendOffset; // offset inside the first vSendMsg already sent
    uint64_t nSendBytes;
    std::deque<std::vector<unsigned char>> vSendMsg;
    RecursiveMutex cs_vSend;

    std::deque<CInv> vRecvGetData;
    std::deque<CNetMessage> vRecvMsg;
    RecursiveMutex cs_vRecvMsg;
    uint64_t nRecvBytes;
    int nRecvVersion;

    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nTimeConnected;
    int64_t nTimeOffset;
    std::atomic<bool> fFirstMessageIsMNAUTH{false};
    CAddress addr;
    std::string addrName;
    CService addrLocal;
    int nVersion;
    // strSubVer is whatever byte array we read from the wire. However, this field is intended
    // to be printed out, displayed to humans in various forms and so on. So we sanitize it and
    // store the sanitized version in cleanSubVer. The original should be used when dealing with
    // the network or wire types and the cleaned string used when displayed or logged.
    std::string strSubVer, cleanSubVer;
    bool fWhitelisted; // This peer can bypass DoS banning.
    bool fOneShot;
    bool fClient;
    bool fInbound;
    bool fNetworkNode;
    bool fSuccessfullyConnected;
    bool fDisconnect;

    // If 'true' this node is a masternode connection.
    std::atomic<bool> m_masternode_connection{false};

    /**
     * If 'true' this node is a masternode probe connection.
     */
    std::atomic<bool> m_masternode_probe_connection{false};

    // If 'true', we identified it as an intra-quorum relay connection.
    std::atomic<bool> m_masternode_iqr_connection{false};

    std::atomic_bool fPauseRecv{false};
    std::atomic_bool fPauseSend{false};

    // We use fRelayTxes for two purposes -
    // a) it allows us to not relay tx invs before receiving the peer's version message
    // b) the peer may tell us in their version message that we should not relay tx invs
    //    until they have initialized their bloom filter.
    bool fRelayTxes;
    CSemaphoreGrant grantOutbound;
    RecursiveMutex cs_filter;
    CBloomFilter* pfilter;
    int nRefCount;
    NodeId id;

protected:
    std::vector<std::string> vecRequestsFulfilled; // keep track of what client has asked for

    // Basic fuzz-testing
    void Fuzz(int nChance); // modifies ssSend

public:
    uint256 hashContinue;
    int nStartingHeight;

    // flood relay
    std::vector<CAddress> vAddrToSend;
    CRollingBloomFilter addrKnown;
    bool fGetAddr;
    std::set<uint256> setKnown;

    // inventory based relay
    CRollingBloomFilter filterInventoryKnown;
    std::vector<CInv> vInventoryToSend;
    RecursiveMutex cs_inventory;
    std::multimap<int64_t, CInv> mapAskFor;
    std::vector<uint256> vBlockRequested;

    // Ping time measurement:
    // The pong reply we're expecting, or 0 if no pong expected.
    uint64_t nPingNonceSent;
    // Time (in usec) the last ping was sent, or 0 if no ping was ever sent.
    int64_t nPingUsecStart;
    // Last measured round-trip time.
    int64_t nPingUsecTime;
    // Whether a ping is requested.
    bool fPingQueued;

    // If true, we will send this peer all quorum related messages,
    // even if the peer is not a member of our quorums.
    std::atomic<bool> qwatch{false};

    CNode(
        NodeId idIn,
        ServiceFlags nLocalServicesIn,
        int nMyStartingHeightIn,
        SOCKET hSocketIn,
        const CAddress& addrIn,
        const std::string& addrNameIn = "",
        bool fInboundIn = false,
        bool fNetworkNodeIn = false);
    ~CNode();

private:
    RecursiveMutex cs_nRefCount;

    CNode(const CNode&);
    void operator=(const CNode&);

    uint64_t nLocalHostNonce;
    ServiceFlags nLocalServices;
    int nMyStartingHeight;
    int nSendVersion;

    // Challenge sent in VERSION to be answered with MNAUTH (only happens between MNs)
    mutable Mutex cs_mnauth;
    uint256 sentMNAuthChallenge GUARDED_BY(cs_mnauth);
    uint256 receivedMNAuthChallenge GUARDED_BY(cs_mnauth);
    uint256 verifiedProRegTxHash GUARDED_BY(cs_mnauth);
    uint256 verifiedPubKeyHash GUARDED_BY(cs_mnauth);

public:
    NodeId GetId() const
    {
        return id;
    }

    uint64_t GetLocalNonce() const
    {
        return nLocalHostNonce;
    }

    ServiceFlags GetLocalServices() const
    {
        return nLocalServices;
    }

    int GetMyStartingHeight() const
    {
        return nMyStartingHeight;
    }

    bool CanRelay() const
    {
        return !m_masternode_connection || m_masternode_iqr_connection;
    }

    uint256 GetSentMNAuthChallenge() const EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        return sentMNAuthChallenge;
    }

    uint256 GetReceivedMNAuthChallenge() const EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        return receivedMNAuthChallenge;
    }

    uint256 GetVerifiedProRegTxHash() const EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        return verifiedProRegTxHash;
    }

    uint256 GetVerifiedPubKeyHash() const EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        return verifiedPubKeyHash;
    }

    void SetSentMNAuthChallenge(const uint256& newSentMNAuthChallenge) EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        sentMNAuthChallenge = newSentMNAuthChallenge;
    }

    void SetReceivedMNAuthChallenge(const uint256& newReceivedMNAuthChallenge) EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        receivedMNAuthChallenge = newReceivedMNAuthChallenge;
    }

    void SetVerifiedProRegTxHash(const uint256& newVerifiedProRegTxHash) EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        verifiedProRegTxHash = newVerifiedProRegTxHash;
    }

    void SetVerifiedPubKeyHash(const uint256& newVerifiedPubKeyHash) EXCLUSIVE_LOCKS_REQUIRED(!cs_mnauth)
    {
        LOCK(cs_mnauth);
        verifiedPubKeyHash = newVerifiedPubKeyHash;
    }

    int GetRefCount()
    {
        LOCK(cs_nRefCount);
        assert(nRefCount >= 0);
        return nRefCount;
    }

    // requires LOCK(cs_vRecvMsg)
    unsigned int GetTotalRecvSize()
    {
        unsigned int total = 0;
        for (const CNetMessage& msg : vRecvMsg)
            total += msg.vRecv.size() + 24;
        return total;
    }

    // requires LOCK(cs_vRecvMsg)
    bool ReceiveMsgBytes(const char* pch, unsigned int nBytes, bool& complete);

    // requires LOCK(cs_vRecvMsg)
    void SetRecvVersion(int nVersionIn)
    {
        nRecvVersion = nVersionIn;
        for (CNetMessage& msg : vRecvMsg)
            msg.SetVersion(nVersionIn);
    }

    void SetSendVersion(int nVersionIn)
    {
        assert(nSendVersion == 0);
        nSendVersion = nVersionIn;
    }

    int GetSendVersion() const
    {
        assert(nSendVersion != 0);
        return nSendVersion;
    }

    CNode* AddRef()
    {
        LOCK(cs_nRefCount);
        nRefCount++;
        return this;
    }

    void Release()
    {
        LOCK(cs_nRefCount);
        nRefCount--;
        assert(nRefCount >= 0);
    }

    void AddAddressKnown(const CAddress& addr)
    {
        addrKnown.insert(addr.GetKey());
    }

    void PushAddress(const CAddress& _addr, FastRandomContext &insecure_rand)
    {
        // Known checking here is only to save space from duplicates.
        // SendMessages will filter it again for knowns that were added
        // after addresses were pushed.
        if (_addr.IsValid() && !addrKnown.contains(_addr.GetKey())) {
            if (vAddrToSend.size() >= MAX_ADDR_TO_SEND) {
                vAddrToSend[insecure_rand.randrange(vAddrToSend.size())] = _addr;
            } else {
                vAddrToSend.push_back(_addr);
            }
        }
    }

    void AddInventoryKnown(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            filterInventoryKnown.insert(inv.hash);
        }
    }

    void PushInventory(const CInv& inv)
    {
        {
            LOCK(cs_inventory);
            if (inv.type != MSG_TX || !filterInventoryKnown.contains(inv.hash))
                vInventoryToSend.push_back(inv);
        }
    }

    void AskFor(const CInv& inv);

    // TODO: Document the postcondition of this function.  Is cs_vSend locked?
    void BeginMessage(const char* pszCommand) EXCLUSIVE_LOCK_FUNCTION(cs_vSend);

    // TODO: Document the precondition of this function.  Is cs_vSend locked?
    void AbortMessage() UNLOCK_FUNCTION(cs_vSend);

    // TODO: Document the precondition of this function.  Is cs_vSend locked?
    void EndMessage() UNLOCK_FUNCTION(cs_vSend);

    void PushMessage(const char* pszCommand)
    {
        try {
            BeginMessage(pszCommand);
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1>
    void PushMessage(const char* pszCommand, const T1& a1)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9, const T10& a10)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9, const T10& a10, const T11& a11)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11, typename T12>
    void PushMessage(const char* pszCommand, const T1& a1, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7, const T8& a8, const T9& a9, const T10& a10, const T11& a11, const T12& a12)
    {
        try {
            BeginMessage(pszCommand);
            ssSend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9 << a10 << a11 << a12;
            EndMessage();
        } catch (...) {
            AbortMessage();
            throw;
        }
    }

    bool HasFulfilledRequest(std::string strRequest)
    {
        for (std::string& type : vecRequestsFulfilled) {
            if (type == strRequest) return true;
        }
        return false;
    }

    void ClearFulfilledRequest(std::string strRequest)
    {
        std::vector<std::string>::iterator it = vecRequestsFulfilled.begin();
        while (it != vecRequestsFulfilled.end()) {
            if ((*it) == strRequest) {
                vecRequestsFulfilled.erase(it);
                return;
            }
            ++it;
        }
    }

    void FulfilledRequest(std::string strRequest)
    {
        if (HasFulfilledRequest(strRequest)) return;
        vecRequestsFulfilled.push_back(strRequest);
    }

    bool IsSubscribed(unsigned int nChannel);
    void Subscribe(unsigned int nChannel, unsigned int nHops = 0);
    void CancelSubscribe(unsigned int nChannel);
    void CloseSocketDisconnect();
    bool DisconnectOldProtocol(int nVersionRequired, std::string strLastCommand = "");

    void copyStats(CNodeStats& stats);
};

/**
 * Interface for message handling
 */
class NetEventsInterface
{
public:
    /** Mutex for anything that is only accessed via the msg processing thread */
    static Mutex g_msgproc_mutex;

    /** Initialize a peer (setup state, queue any initial messages) */
    virtual void InitializeNode(CNode& node, ServiceFlags our_services) = 0;

    /** Handle removal of a peer (clear state) */
    virtual void FinalizeNode(const CNode& node) = 0;

    /**
     * Process protocol messages received from a given node.
     *
     * @param[in] pnode      The node which we have received messages from.
     * @param[in] interrupt  Interrupt condition for processing threads.
     * @return               True if there is more work to be done.
     */
    virtual bool ProcessMessages(CNode* pnode, std::atomic<bool>& interrupt) EXCLUSIVE_LOCKS_REQUIRED(g_msgproc_mutex) = 0;

    /**
     * Send queued protocol messages to a given node.
     *
     * @param[in] pnode  The node which we are sending messages to.
     * @return           True if there is more work to be done.
     */
    virtual bool SendMessages(CNode* pnode) EXCLUSIVE_LOCKS_REQUIRED(g_msgproc_mutex) = 0;

protected:
    ~NetEventsInterface() = default;
};

class CExplicitNetCleanup
{
public:
    static void callCleanup();
};

void DumpBanlist();

#endif // SCHILLINGCOIN_NET_H