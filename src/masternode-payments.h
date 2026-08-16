// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODE_PAYMENTS_H
#define MASTERNODE_PAYMENTS_H

#include "key.h"
#include "main.h"
#include "masternode.h"

extern RecursiveMutex cs_vecPayments;
extern RecursiveMutex cs_mapMasternodeBlocks;
extern RecursiveMutex cs_mapMasternodePayeeVotes;

class CMasternodePayments;
class CMasternodePaymentWinner;
class CMasternodeBlockPayees;

extern CMasternodePayments masternodePayments;

#define MNPAYMENTS_SIGNATURES_REQUIRED 6
#define MNPAYMENTS_SIGNATURES_TOTAL 10

void ProcessMessageMasternodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
bool IsBlockPayeeValid(const CBlock& block, int nBlockHeight);
std::string GetRequiredPaymentsString(int nBlockHeight);
bool IsBlockValueValid(const CBlock& block, CAmount nExpectedValue, CAmount nMinted);
void FillBlockPayee(CMutableTransaction& txNew, CAmount nFees, bool fProofOfStake);

void DumpMasternodePayments();

/** Save Masternode Payment Data (mnpayments.dat)
 */
class CMasternodePaymentDB
{
private:
    boost::filesystem::path pathDB;
    std::string strMagicMessage;

public:
    enum ReadResult {
        Ok,
        FileError,
        HashReadError,
        IncorrectHash,
        IncorrectMagicMessage,
        IncorrectMagicNumber,
        IncorrectFormat
    };

    CMasternodePaymentDB();
    bool Write(const CMasternodePayments& objToSave);
    ReadResult Read(CMasternodePayments& objToLoad, bool fDryRun = false);
};

class CMasternodePayee
{
public:
    CScript scriptPubKey;
    int nVotes;

    CMasternodePayee()
        : scriptPubKey(),
          nVotes(0)
    {}

    CMasternodePayee(CScript payee, int nVotesIn)
        : scriptPubKey(payee),
          nVotes(nVotesIn)
    {}

    // Copy constructor
    CMasternodePayee(const CMasternodePayee& other)
        : scriptPubKey(other.scriptPubKey),
          nVotes(other.nVotes)
    {}

    // Move constructor
    CMasternodePayee(CMasternodePayee&& other) noexcept
        : scriptPubKey(std::move(other.scriptPubKey)),
          nVotes(other.nVotes)
    {}

    // Copy assignment
    CMasternodePayee& operator=(const CMasternodePayee& other)
    {
        if (this != &other) {
            scriptPubKey = other.scriptPubKey;
            nVotes       = other.nVotes;
        }
        return *this;
    }

    // Move assignment
    CMasternodePayee& operator=(CMasternodePayee&& other) noexcept
    {
        if (this != &other) {
            scriptPubKey = std::move(other.scriptPubKey);
            nVotes       = other.nVotes;
        }
        return *this;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(scriptPubKey);
        READWRITE(nVotes);
    }
};

// Keep track of votes for payees from masternodes
class CMasternodeBlockPayees
{
public:
    int nBlockHeight;
    std::vector<CMasternodePayee> vecPayments;

    CMasternodeBlockPayees()
        : nBlockHeight(0),
          vecPayments()
    {}

    CMasternodeBlockPayees(int nBlockHeightIn)
        : nBlockHeight(nBlockHeightIn),
          vecPayments()
    {}

    // Copy constructor
    CMasternodeBlockPayees(const CMasternodeBlockPayees& other)
        : nBlockHeight(other.nBlockHeight),
          vecPayments(other.vecPayments)
    {}

    // Move constructor
    CMasternodeBlockPayees(CMasternodeBlockPayees&& other) noexcept
        : nBlockHeight(other.nBlockHeight),
          vecPayments(std::move(other.vecPayments))
    {}

    // Copy assignment
    CMasternodeBlockPayees& operator=(const CMasternodeBlockPayees& other)
    {
        if (this != &other) {
            nBlockHeight = other.nBlockHeight;
            vecPayments  = other.vecPayments;
        }
        return *this;
    }

    // Move assignment
    CMasternodeBlockPayees& operator=(CMasternodeBlockPayees&& other) noexcept
    {
        if (this != &other) {
            nBlockHeight = other.nBlockHeight;
            vecPayments  = std::move(other.vecPayments);
        }
        return *this;
    }

    void AddPayee(CScript payeeIn, int nIncrement)
    {
        LOCK(cs_vecPayments);

        for (CMasternodePayee& payee : vecPayments) {
            if (payee.scriptPubKey == payeeIn) {
                payee.nVotes += nIncrement;
                return;
            }
        }

        CMasternodePayee c(payeeIn, nIncrement);
        vecPayments.push_back(c);
    }

    bool GetPayee(CScript& payee)
    {
        LOCK(cs_vecPayments);

        int nVotes = -1;
        for (CMasternodePayee& p : vecPayments) {
            if (p.nVotes > nVotes) {
                payee = p.scriptPubKey;
                nVotes = p.nVotes;
            }
        }

        return (nVotes > -1);
    }

    bool HasPayeeWithVotes(CScript payee, int nVotesReq)
    {
        LOCK(cs_vecPayments);

        for (CMasternodePayee& p : vecPayments) {
            if (p.nVotes >= nVotesReq && p.scriptPubKey == payee) return true;
        }

        return false;
    }

    bool IsTransactionValid(const CTransaction& txNew);
    std::string GetRequiredPaymentsString();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nBlockHeight);
        READWRITE(vecPayments);
    }
};

// for storing the winning payments
class CMasternodePaymentWinner : public CSignedMessage
{
public:
    CTxIn vinMasternode;
    int nBlockHeight;
    CScript payee;

    CMasternodePaymentWinner()
        : CSignedMessage(),
          vinMasternode(),
          nBlockHeight(0),
          payee()
    {}

    CMasternodePaymentWinner(CTxIn vinIn)
        : CSignedMessage(),
          vinMasternode(vinIn),
          nBlockHeight(0),
          payee()
    {}

    // Copy constructor
    CMasternodePaymentWinner(const CMasternodePaymentWinner& other)
        : CSignedMessage(other),
          vinMasternode(other.vinMasternode),
          nBlockHeight(other.nBlockHeight),
          payee(other.payee)
    {}

    // Move constructor
    CMasternodePaymentWinner(CMasternodePaymentWinner&& other) noexcept
        : CSignedMessage(std::move(other)),
          vinMasternode(std::move(other.vinMasternode)),
          nBlockHeight(other.nBlockHeight),
          payee(std::move(other.payee))
    {}

    // Copy assignment
    CMasternodePaymentWinner& operator=(const CMasternodePaymentWinner& other)
    {
        if (this != &other) {
            CSignedMessage::operator=(other);
            vinMasternode = other.vinMasternode;
            nBlockHeight  = other.nBlockHeight;
            payee         = other.payee;
        }
        return *this;
    }

    // Move assignment
    CMasternodePaymentWinner& operator=(CMasternodePaymentWinner&& other) noexcept
    {
        if (this != &other) {
            CSignedMessage::operator=(std::move(other));
            vinMasternode = std::move(other.vinMasternode);
            nBlockHeight  = other.nBlockHeight;
            payee         = std::move(other.payee);
        }
        return *this;
    }

    uint256 GetHash() const;

    // override CSignedMessage functions
    uint256 GetSignatureHash() const override { return GetHash(); }
    std::string GetStrMessage() const override;
    const CTxIn GetVin() const override { return vinMasternode; };

    bool IsValid(CNode* pnode, std::string& strError);
    void Relay();

    void AddPayee(CScript payeeIn)
    {
        payee = payeeIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vinMasternode);
        READWRITE(nBlockHeight);
        READWRITE(payee);
        READWRITE(vchSig);
        try {
            READWRITE(nMessVersion);
        } catch (...) {
            nMessVersion = MessageVersion::MESS_VER_STRMESS;
        }
    }

    std::string ToString()
    {
        std::string ret = "";
        ret += vinMasternode.ToString();
        ret += ", " + std::to_string(nBlockHeight);
        ret += ", " + payee.ToString();
        ret += ", " + std::to_string((int)vchSig.size());
        return ret;
    }
};

//
// Masternode Payments Class
// Keeps track of who should get paid for which blocks
//

class CMasternodePayments
{
private:
    int nSyncedFromPeer;
    int nLastBlockHeight;

public:
    std::map<uint256, CMasternodePaymentWinner> mapMasternodePayeeVotes;
    std::map<int, CMasternodeBlockPayees> mapMasternodeBlocks;
    std::map<COutPoint, int> mapMasternodesLastVote; // prevout, nBlockHeight

    CMasternodePayments()
        : nSyncedFromPeer(0),
          nLastBlockHeight(0),
          mapMasternodePayeeVotes(),
          mapMasternodeBlocks(),
          mapMasternodesLastVote()
    {}

    // Copy constructor
    CMasternodePayments(const CMasternodePayments& other)
        : nSyncedFromPeer(other.nSyncedFromPeer),
          nLastBlockHeight(other.nLastBlockHeight),
          mapMasternodePayeeVotes(other.mapMasternodePayeeVotes),
          mapMasternodeBlocks(other.mapMasternodeBlocks),
          mapMasternodesLastVote(other.mapMasternodesLastVote)
    {}

    // Move constructor
    CMasternodePayments(CMasternodePayments&& other) noexcept
        : nSyncedFromPeer(other.nSyncedFromPeer),
          nLastBlockHeight(other.nLastBlockHeight),
          mapMasternodePayeeVotes(std::move(other.mapMasternodePayeeVotes)),
          mapMasternodeBlocks(std::move(other.mapMasternodeBlocks)),
          mapMasternodesLastVote(std::move(other.mapMasternodesLastVote))
    {}

    // Copy assignment
    CMasternodePayments& operator=(const CMasternodePayments& other)
    {
        if (this != &other) {
            nSyncedFromPeer        = other.nSyncedFromPeer;
            nLastBlockHeight       = other.nLastBlockHeight;
            mapMasternodePayeeVotes = other.mapMasternodePayeeVotes;
            mapMasternodeBlocks     = other.mapMasternodeBlocks;
            mapMasternodesLastVote  = other.mapMasternodesLastVote;
        }
        return *this;
    }

    // Move assignment
    CMasternodePayments& operator=(CMasternodePayments&& other) noexcept
    {
        if (this != &other) {
            nSyncedFromPeer        = other.nSyncedFromPeer;
            nLastBlockHeight       = other.nLastBlockHeight;
            mapMasternodePayeeVotes = std::move(other.mapMasternodePayeeVotes);
            mapMasternodeBlocks     = std::move(other.mapMasternodeBlocks);
            mapMasternodesLastVote  = std::move(other.mapMasternodesLastVote);
        }
        return *this;
    }

    void Clear()
    {
        LOCK2(cs_mapMasternodeBlocks, cs_mapMasternodePayeeVotes);
        mapMasternodeBlocks.clear();
        mapMasternodePayeeVotes.clear();
    }

    bool AddWinningMasternode(CMasternodePaymentWinner& winner);
    bool ProcessBlock(int nBlockHeight);

    void Sync(CNode* node, int nCountNeeded);
    void CleanPaymentList();
    int LastPayment(CMasternode& mn);

    bool GetBlockPayee(int nBlockHeight, CScript& payee);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    bool IsScheduled(CMasternode& mn, int nNotBlockHeight);

    bool CanVote(COutPoint outMasternode, int nBlockHeight)
    {
        LOCK(cs_mapMasternodePayeeVotes);

        if (mapMasternodesLastVote.count(outMasternode)) {
            if (mapMasternodesLastVote[outMasternode] == nBlockHeight) {
                return false;
            }
        }

        // record this masternode voted
        mapMasternodesLastVote[outMasternode] = nBlockHeight;
        return true;
    }

    int GetMinMasternodePaymentsProto();
    void ProcessMessageMasternodePayments(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txNew, int64_t nFees, bool fProofOfStake);
    std::string ToString() const;
    int GetOldestBlock();
    int GetNewestBlock();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapMasternodePayeeVotes);
        READWRITE(mapMasternodeBlocks);
    }
};

#endif