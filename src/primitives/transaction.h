// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_PRIMITIVES_TRANSACTION_H
#define SCHILLINGCOIN_PRIMITIVES_TRANSACTION_H

#include "consensus/amount.h"
#include "optional.h"
#include "primitives/transaction_identifier.h" // IWYU pragma: export
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

#include <list>
#include <memory>
#include <tuple>
#include <utility>

class CTransaction;

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    Txid hash;
    uint32_t n;

    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    COutPoint() : n(NULL_INDEX) {}
    COutPoint(const Txid& hashIn, uint32_t nIn): hash(hashIn), n(nIn) { }

    SERIALIZE_METHODS(COutPoint, obj) { READWRITE(obj.hash, obj.n); }

    void SetNull() { hash.SetNull(); n = NULL_INDEX; }
    bool IsNull() const { return hash.IsNull() && n == NULL_INDEX; }

    bool IsMasternodeReward(const CTransaction* tx) const;

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return std::tie(a.hash, a.n) < std::tie(b.hash, b.n);
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return a.hash == b.hash && a.n == b.n;
    }

    std::string ToString() const;
    std::string ToStringShort() const;
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
    CScriptWitness scriptWitness; //!< Only serialized through CTransaction
    CScript prevPubKey;

    CTxIn()
    {
        nSequence = std::numeric_limits<unsigned int>::max();
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=std::numeric_limits<unsigned int>::max());
    CTxIn(Txid hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=std::numeric_limits<uint32_t>::max());

    // Copy constructor
    CTxIn(const CTxIn& other)
        : prevout(other.prevout),
          scriptSig(other.scriptSig),
          nSequence(other.nSequence),
          scriptWitness(other.scriptWitness),
          prevPubKey(other.prevPubKey)
    {}

    // Move constructor
    CTxIn(CTxIn&& other) noexcept
        : prevout(std::move(other.prevout)),
          scriptSig(std::move(other.scriptSig)),
          nSequence(other.nSequence),
          scriptWitness(std::move(other.scriptWitness)),
          prevPubKey(std::move(other.prevPubKey))
    {}

    // Copy assignment
    CTxIn& operator=(const CTxIn& other)
    {
        if (this != &other) {
            prevout      = other.prevout;
            scriptSig    = other.scriptSig;
            nSequence    = other.nSequence;
            scriptWitness = other.scriptWitness;
            prevPubKey   = other.prevPubKey;
        }
        return *this;
    }

    // Move assignment
    CTxIn& operator=(CTxIn&& other) noexcept
    {
        if (this != &other) {
            prevout      = std::move(other.prevout);
            scriptSig    = std::move(other.scriptSig);
            nSequence    = other.nSequence;
            scriptWitness = std::move(other.scriptWitness);
            prevPubKey   = std::move(other.prevPubKey);
        }
        return *this;
    }

    SERIALIZE_METHODS(CTxIn, obj)
    {
        READWRITE(obj.prevout, obj.scriptSig, obj.nSequence);
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

    SERIALIZE_METHODS(CTxOut, obj)
    {
        READWRITE(obj.nValue, obj.scriptPubKey);
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
    TRANSACTION_PROVIDER_REGISTER = 1,
    TRANSACTION_PROVIDER_UPDATE_SERVICE = 2,
    TRANSACTION_PROVIDER_UPDATE_REGISTRAR = 3,
    TRANSACTION_PROVIDER_UPDATE_REVOKE = 4,
    TRANSACTION_COINBASE = 5,
    TRANSACTION_QUORUM_COMMITMENT = 6,
    TRANSACTION_MNHF_SIGNAL = 7,

    // Transaction type values 8 and 9 are intentionally reserved to preserve
    // the authoritative Dash special-transaction numeric namespace. SCH does
    // not implement AssetLock or AssetUnlock transactions.

    TRANSACTION_PROVIDER_DISSOLVE = 10,
    TRANSACTION_PROVIDER_UPDATE_SHARE = 11,
    TRANSACTION_PROVIDER_UPDATE_SHARED_REGISTRAR = 12,
};

struct CMutableTransaction;

struct TransactionSerParams {
    const bool allow_witness;
    SER_PARAMS_OPFUNC
};

inline constexpr TransactionSerParams TX_WITH_WITNESS{.allow_witness = true};
inline constexpr TransactionSerParams TX_NO_WITNESS{.allow_witness = false};

template<typename Stream, typename TxType>
void UnserializeTransaction(TxType& tx, Stream& s, const TransactionSerParams& params)
{
    const bool allow_witness = params.allow_witness;

    uint32_t n32bitVersion;
    s >> n32bitVersion;
    tx.nVersion = static_cast<int32_t>(n32bitVersion & 0xffff);
    tx.nType = static_cast<uint16_t>((n32bitVersion >> 16) & 0xffff);

    unsigned char flags = 0;
    tx.vin.clear();
    tx.vout.clear();

    s >> tx.vin;
    if (tx.vin.empty() && allow_witness) {
        s >> flags;
        if (flags != 0) {
            s >> tx.vin;
            s >> tx.vout;
        }
    } else {
        s >> tx.vout;
    }

    if ((flags & 1) && allow_witness) {
        flags ^= 1;
        for (auto& input : tx.vin) {
            s >> input.scriptWitness.stack;
        }
        if (!tx.HasWitness()) {
            throw std::ios_base::failure("Superfluous witness record");
        }
    }

    if (flags) {
        throw std::ios_base::failure("Unknown transaction optional data");
    }

    s >> tx.nLockTime;

    if (tx.HasExtraPayloadField()) {
        s >> tx.vExtraPayload;
    } else {
        tx.vExtraPayload.clear();
    }
}

template<typename Stream, typename TxType>
void SerializeTransaction(const TxType& tx, Stream& s, const TransactionSerParams& params)
{
    const bool allow_witness = params.allow_witness;

    const uint32_t n32bitVersion =
        static_cast<uint16_t>(tx.nVersion) |
        (static_cast<uint32_t>(tx.nType) << 16);
    s << n32bitVersion;

    unsigned char flags = 0;
    if (allow_witness && tx.HasWitness()) {
        flags |= 1;
    }

    if (flags) {
        const std::vector<CTxIn> vin_dummy;
        s << vin_dummy;
        s << flags;
    }

    s << tx.vin;
    s << tx.vout;

    if (flags & 1) {
        for (const auto& input : tx.vin) {
            s << input.scriptWitness.stack;
        }
    }

    s << tx.nLockTime;

    if (tx.HasExtraPayloadField()) {
        s << tx.vExtraPayload;
    }
}

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
public:
    static const int32_t CURRENT_VERSION = 1;
    static const int32_t SPECIAL_VERSION = 3;

    const int32_t nVersion;
    const uint16_t nType;
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    const uint32_t nLockTime;
    std::vector<uint8_t> vExtraPayload;

private:
    /** Memory only. */
    const bool m_has_witness;
    const Txid hash;
    const Wtxid m_witness_hash;

    bool ComputeHasWitness() const;
    Txid ComputeHash() const;
    Wtxid ComputeWitnessHash() const;

public:
    CTransaction();
    CTransaction(const CMutableTransaction& tx);

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        SerializeTransaction(*this, s, s.template GetParams<TransactionSerParams>());
    }

    /**
     * This deserializing constructor is provided instead of an Unserialize
     * method. Unserialize is not possible without overwriting const fields.
     */
    template <typename Stream>
    CTransaction(deserialize_type, const TransactionSerParams& params, Stream& s)
        : CTransaction(CMutableTransaction(deserialize, params, s))
    {
    }

    template <typename Stream>
    CTransaction(deserialize_type, Stream& s)
        : CTransaction(CMutableTransaction(deserialize, s))
    {
    }

    // Explicit copy constructor to avoid deprecated implicit copy.
    CTransaction(const CTransaction& tx) :
        nVersion(tx.nVersion),
        nType(tx.nType),
        vin(tx.vin),
        vout(tx.vout),
        nLockTime(tx.nLockTime),
        vExtraPayload(tx.vExtraPayload),
        m_has_witness(tx.m_has_witness),
        hash(tx.hash),
        m_witness_hash(tx.m_witness_hash)
    {
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

    const Txid& GetHash() const
    {
        return hash;
    }

    const Wtxid& GetWitnessHash() const
    {
        return m_witness_hash;
    }

    bool HasWitness() const
    {
        return m_has_witness;
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

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        SerializeTransaction(*this, s, s.template GetParams<TransactionSerParams>());
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        UnserializeTransaction(*this, s, s.template GetParams<TransactionSerParams>());
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, const TransactionSerParams& params, Stream& s)
    {
        UnserializeTransaction(*this, s, params);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s)
    {
        Unserialize(s);
    }

    bool IsSpecialTxVersion() const
    {
        return nVersion >= CTransaction::SPECIAL_VERSION;
    }

    bool HasExtraPayloadField() const
    {
        return IsSpecialTxVersion() && nType != TRANSACTION_NORMAL;
    }

    Txid GetHash() const;

    bool HasWitness() const
    {
        for (const auto& input : vin) {
            if (!input.scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }

    std::string ToString() const;
};

typedef std::shared_ptr<const CTransaction> CTransactionRef;

template <typename Tx>
static inline CTransactionRef MakeTransactionRef(Tx&& txIn)
{
    return std::make_shared<const CTransaction>(std::forward<Tx>(txIn));
}

#endif // SCHILLINGCOIN_PRIMITIVES_TRANSACTION_H