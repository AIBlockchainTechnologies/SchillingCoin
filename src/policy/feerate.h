// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2017-2020 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_POLICY_FEERATE_H
#define SCHILLINGCOIN_POLICY_FEERATE_H

#include "consensus/amount.h"
#include "serialize.h"

#include <concepts>
#include <cstdint>
#include <string>

inline const std::string CURRENCY_UNIT = "SCH";
inline const std::string CURRENCY_ATOM = "sat";

/* Used to determine type of fee estimation requested */
enum class FeeEstimateMode {
    UNSET,        //!< Use default settings based on other criteria
    ECONOMICAL,   //!< Force estimateSmartFee to use non-conservative estimates
    CONSERVATIVE, //!< Force estimateSmartFee to use conservative estimates
    SCH_KB,       //!< Use SCH/kB fee rate unit
    SAT_B,        //!< Use sat/B fee rate unit
};

/**
 * Fee rate in satoshis per kilobyte: CAmount / kB
 */
class CFeeRate
{
private:
    /** Fee rate in sat/kB (satoshis per 1000 bytes) */
    CAmount nSatoshisPerK;

public:
    /** Fee rate of 0 satoshis per kB */
    CFeeRate() : nSatoshisPerK(0) {}

    template<std::integral I> // Disallow silent float -> int conversion
    explicit CFeeRate(const I _nSatoshisPerK) : nSatoshisPerK(_nSatoshisPerK)
    {
    }

    /**
     * Construct a fee rate from a fee in satoshis and a size in bytes.
     */
    CFeeRate(const CAmount& nFeePaid, uint32_t num_bytes);

    /**
     * Return the fee in satoshis for the given size in bytes.
     * If the calculated fee would have fractional satoshis, the returned fee
     * is rounded up to the nearest satoshi for positive fee rates.
     */
    CAmount GetFee(uint32_t num_bytes) const;

    /** Return the fee in satoshis for a size of 1000 bytes. */
    CAmount GetFeePerK() const { return nSatoshisPerK; }

    friend bool operator<(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK < b.nSatoshisPerK; }
    friend bool operator>(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK > b.nSatoshisPerK; }
    friend bool operator==(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK == b.nSatoshisPerK; }
    friend bool operator<=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK <= b.nSatoshisPerK; }
    friend bool operator>=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK >= b.nSatoshisPerK; }
    friend bool operator!=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK != b.nSatoshisPerK; }

    CFeeRate& operator+=(const CFeeRate& a)
    {
        nSatoshisPerK += a.nSatoshisPerK;
        return *this;
    }

    std::string ToString(const FeeEstimateMode& fee_estimate_mode = FeeEstimateMode::SCH_KB) const;

    friend CFeeRate operator*(const CFeeRate& f, int a) { return CFeeRate(a * f.nSatoshisPerK); }
    friend CFeeRate operator*(int a, const CFeeRate& f) { return CFeeRate(a * f.nSatoshisPerK); }

    SERIALIZE_METHODS(CFeeRate, obj) { READWRITE(obj.nSatoshisPerK); }
};

#endif // SCHILLINGCOIN_POLICY_FEERATE_H