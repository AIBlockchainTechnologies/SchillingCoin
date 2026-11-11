// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2018-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/bitcoinconsensus.h>

#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <span.h>

#include <cstring>
#include <ios>
#include <string>

namespace {

/** A stream that deserializes a single CTransaction one time. */
class TxInputStream
{
public:
    TxInputStream(const unsigned char* tx_to, size_t tx_to_len) :
        m_data(tx_to),
        m_remaining(tx_to_len)
    {
    }

    void read(Span<std::byte> dst)
    {
        if (dst.size() > m_remaining) {
            throw std::ios_base::failure(std::string(__func__) + ": end of data");
        }
        if (dst.data() == nullptr) {
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");
        }
        if (m_data == nullptr) {
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");
        }

        std::memcpy(dst.data(), m_data, dst.size());
        m_remaining -= dst.size();
        m_data += dst.size();
    }

    template <typename T>
    TxInputStream& operator>>(T&& obj)
    {
        ::Unserialize(*this, obj);
        return *this;
    }

private:
    const unsigned char* m_data;
    size_t m_remaining;
};

inline int set_error(bitcoinconsensus_error* ret, bitcoinconsensus_error serror)
{
    if (ret) {
        *ret = serror;
    }
    return 0;
}

} // namespace

int bitcoinconsensus_verify_script(const unsigned char* scriptPubKey,
                                   unsigned int scriptPubKeyLen,
                                   const unsigned char* txTo,
                                   unsigned int txToLen,
                                   unsigned int nIn,
                                   unsigned int flags,
                                   bitcoinconsensus_error* err)
{
    try {
        TxInputStream stream(txTo, txToLen);
        const CTransaction tx(deserialize, TX_WITH_WITNESS, stream);

        if (nIn >= tx.vin.size()) {
            return set_error(err, bitcoinconsensus_ERR_TX_INDEX);
        }

        if (GetSerializeSize(TX_WITH_WITNESS(tx)) != txToLen) {
            return set_error(err, bitcoinconsensus_ERR_TX_SIZE_MISMATCH);
        }

        // Regardless of the verification result, the transaction did not
        // produce an API-level deserialization or input-index error.
        set_error(err, bitcoinconsensus_ERR_OK);

        const PrecomputedTransactionData txdata(tx);
        return VerifyScript(
            tx.vin[nIn].scriptSig,
            CScript(scriptPubKey, scriptPubKey + scriptPubKeyLen),
            flags,
            TransactionSignatureChecker(
                &tx,
                nIn,
                CAmount{0},
                txdata,
                MissingDataBehavior::FAIL),
            nullptr);
    } catch (const std::exception&) {
        return set_error(err, bitcoinconsensus_ERR_TX_DESERIALIZE);
    }
}

unsigned int bitcoinconsensus_version()
{
    return BITCOINCONSENSUS_API_VER;
}