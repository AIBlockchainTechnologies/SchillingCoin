// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_PRIMITIVES_TRANSACTION_H
#define SCHILLINGCOIN_PRIMITIVES_TRANSACTION_H

#include "amount.h"
#include "optional.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

#include <list>

class CTransaction;

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, uint32_t nIn) { hash = hashIn; n = nIn; }

    // Copy constructor
    COutPoint(const COutPoint& other)
        : hash(other.hash), n(other.n)
    {}

    // Move constructor
    COutPoint(COutPoint&& other) noexcept
        : hash(std::move(other.hash)), n(other.n)
    {}

    // Copy assignment
    COutPoint& operator=(const COutPoint& other)
    {
        if (this != &other) {
            hash = other.hash;
            n    = other.n;
        }
        return *this;
    }

    // Move assignment
    COutPoint& operator=(COutPoint&& other) noexcept
    {
        if (this != &other) {
            hash = std::move(other.hash);
            n    = other.n;
        }
        return *this;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(FLATDATA(*this));
    }

    void SetNull() { hash.SetNull(); n = (uint32_t) -1; }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t) -1); }
    bool IsMasternodeReward(const CTransaction* tx) const;

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
    std::string ToStringShort() const;

    uint256 GetHash();
};

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
    CScript prevPubKey;

    CTxIn()
    {
        nSequence = std::numeric_limits<unsigned int>::max();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=std::numeric_limits<unsigned int>::max());
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=std::numeric_limits<uint32_t>::max());

    // Copy constructor
    CTxIn(const CTxIn& other)
        : prevout(other.prevout),
          scriptSig(other.scriptSig),
          nSequence(other.nSequence),
          prevPubKey(other.prevPubKey)
    {}

    // Move constructor
    CTxIn(CTxIn&& other) noexcept
        : prevout(std::move(other.prevout)),
          scriptSig(std::move(other.scriptSig)),
          nSequence(other.nSequence),
          prevPubKey(std::move(other.prevPubKey))
    {}

    // Copy assignment
    CTxIn& operator=(const CTxIn& other)
    {
        if (this != &other) {
            prevout    = other.prevout;
            scriptSig  = other.scriptSig;
            nSequence  = other.nSequence;
            prevPubKey = other.prevPubKey;
        }
        return *this;
    }

    // Move assignment
    CTxIn& operator=(CTxIn&& other) noexcept
    {
        if (this != &other) {
            prevout    = std::move(other.prevout);
            scriptSig  = std::move(other.scriptSig);
            nSequence  = other.nSequence;
            prevPubKey = std::move(other.prevPubKey);
        }
        return *this;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    }

    bool IsFinal() const
    {
        return (nSequence == std::numeric_limits<uint32_t>::max());
    }

    bool IsZerocoinSpend() const;

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;
    int nRounds;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);

    // Copy constructor
    CTxOut(const CTxOut& other)
        : nValue(other.nValue),
          scriptPubKey(other.scriptPubKey),
          nRounds(other.nRounds)
    {}

    // Move constructor
    CTxOut(CTxOut&& other) noexcept
        : nValue(other.nValue),
          scriptPubKey(std::move(other.scriptPubKey)),
          nRounds(other.nRounds)
    {}

    // Copy assignment
    CTxOut& operator=(const CTxOut& other)
    {
        if (this != &other) {
            nValue       = other.nValue;
            scriptPubKey = other.scriptPubKey;
            nRounds      = other.nRounds;
        }
        return *this;
    }

    // Move assignment
    CTxOut& operator=(CTxOut&& other) noexcept
    {
        if (this != &other) {
            nValue       = other.nValue;
            scriptPubKey = std::move(other.scriptPubKey);
            nRounds      = other.nRounds;
        }
        return *this;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
        nRounds = -10; // an initial value, should be no way to get this by calculations
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    void SetEmpty()
    {
        nValue = 0;
        scriptPubKey.clear();
    }

    bool IsEmpty() const
    {
        return (nValue == 0 && scriptPubKey.empty());
    }

    uint256 GetHash() const;

    bool IsZerocoinMint() const;
    CAmount GetZerocoinMinted() const;

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey &&
                a.nRounds      == b.nRounds);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

enum TransactionType : uint16_t
{
    TRANSACTION_NORMAL = 0,
};

struct CMutableTransaction;

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
private:
    /** Memory only. */
    const uint256 hash;
    void UpdateHash() const;

public:
    static const int32_t CURRENT_VERSION = 1;
    static const int32_t SPECIAL_VERSION = 3;

    const int32_t nVersion;
    const uint16_t nType;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    const uint32_t nLockTime;
    std::vector<uint8_t> vExtraPayload;

    CTransaction();
    CTransaction(const CMutableTransaction &tx);

    // Explicit copy constructor to avoid deprecated implicit copy.
    CTransaction(const CTransaction& tx) :
        hash(tx.hash),
        nVersion(tx.nVersion),
        nType(tx.nType),
        vin(tx.vin),
        vout(tx.vout),
        nLockTime(tx.nLockTime),
        vExtraPayload(tx.vExtraPayload)
    {
    }

    // Explicit copy assignment operator.
    CTransaction& operator=(const CTransaction& tx)
    {
        if (this != &tx) {
            // Const members are updated through the existing complete-object
            // assignment compatibility path.
            *const_cast<int32_t*>(&nVersion)   = tx.nVersion;
            *const_cast<uint16_t*>(&nType)     = tx.nType;
            vin                                = tx.vin;
            vout                               = tx.vout;
            *const_cast<uint32_t*>(&nLockTime) = tx.nLockTime;
            vExtraPayload                      = tx.vExtraPayload;
            *const_cast<uint256*>(&hash)       = tx.hash;
        }
        return *this;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        uint32_t n32bitVersion =
            static_cast<uint16_t>(this->nVersion) |
            (static_cast<uint32_t>(this->nType) << 16);

        READWRITE(n32bitVersion);

        if (ser_action.ForRead()) {
            *const_cast<int32_t*>(&this->nVersion) =
                static_cast<int32_t>(n32bitVersion & 0xffff);
            *const_cast<uint16_t*>(&this->nType) =
                static_cast<uint16_t>((n32bitVersion >> 16) & 0xffff);
        }

        nVersion = this->nVersion;

        READWRITE(*const_cast<std::vector<CTxIn>*>(&vin));
        READWRITE(*const_cast<std::vector<CTxOut>*>(&vout));
        READWRITE(*const_cast<uint32_t*>(&nLockTime));

        if (HasExtraPayloadField()) {
            READWRITE(vExtraPayload);
        } else if (ser_action.ForRead()) {
            vExtraPayload.clear();
        }

        if (ser_action.ForRead())
            UpdateHash();
    }

    bool IsSpecialTxVersion() const
    {
        return nVersion >= SPECIAL_VERSION;
    }

    bool HasExtraPayloadField() const
    {
        return IsSpecialTxVersion() && nType != TRANSACTION_NORMAL;
    }

    bool IsNull() const
    {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const
    {
        return hash;
    }

    CAmount GetValueOut() const;
    double ComputePriority(double dPriorityInputs, unsigned int nTxSize=0) const;
    unsigned int CalculateModifiedSize(unsigned int nTxSize=0) const;

    bool HasZerocoinSpendInputs() const;
    bool HasZerocoinPublicSpendInputs() const;
    bool HasZerocoinMintOutputs() const;

    bool ContainsZerocoins() const
    {
        return HasZerocoinSpendInputs() || HasZerocoinPublicSpendInputs() || HasZerocoinMintOutputs();
    }

    CAmount GetZerocoinMinted() const;
    CAmount GetZerocoinSpent() const;
    int GetZerocoinMintCount() const;

    bool UsesUTXO(const COutPoint out);
    std::list<COutPoint> GetOutPoints() const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull() && !ContainsZerocoins());
    }

    bool IsCoinStake() const;
    bool CheckColdStake(const CScript& script) const;
    bool HasP2CSOutputs() const;

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    unsigned int GetTotalSize() const;

    std::string ToString() const;
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    int32_t nVersion;
    uint16_t nType;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    uint32_t nLockTime;
    std::vector<uint8_t> vExtraPayload;

    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        uint32_t n32bitVersion =
            static_cast<uint16_t>(this->nVersion) |
            (static_cast<uint32_t>(this->nType) << 16);

        READWRITE(n32bitVersion);

        if (ser_action.ForRead()) {
            this->nVersion =
                static_cast<int32_t>(n32bitVersion & 0xffff);
            this->nType =
                static_cast<uint16_t>((n32bitVersion >> 16) & 0xffff);
        }

        nVersion = this->nVersion;

        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);

        if (HasExtraPayloadField()) {
            READWRITE(vExtraPayload);
        } else if (ser_action.ForRead()) {
            vExtraPayload.clear();
        }
    }

    bool IsSpecialTxVersion() const
    {
        return nVersion >= CTransaction::SPECIAL_VERSION;
    }

    bool HasExtraPayloadField() const
    {
        return IsSpecialTxVersion() && nType != TRANSACTION_NORMAL;
    }

    uint256 GetHash() const;
    std::string ToString() const;
};

#endif // SCHILLINGCOIN_PRIMITIVES_TRANSACTION_H