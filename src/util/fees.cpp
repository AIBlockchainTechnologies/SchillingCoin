// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "policy/fees.h"
#include "util/fees.h"
#include "util/string.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string ToUpperString(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

const std::vector<std::pair<std::string, FeeEstimateMode>>& FeeModeMap()
{
    static const std::vector<std::pair<std::string, FeeEstimateMode>> fee_modes{
        {"unset", FeeEstimateMode::UNSET},
        {"economical", FeeEstimateMode::ECONOMICAL},
        {"conservative", FeeEstimateMode::CONSERVATIVE},
    };
    return fee_modes;
}

} // namespace

bool FeeModeFromString(const std::string& mode_string, FeeEstimateMode& fee_estimate_mode)
{
    const std::string search_key = ToUpperString(mode_string);

    for (const auto& entry : FeeModeMap()) {
        if (ToUpperString(entry.first) == search_key) {
            fee_estimate_mode = entry.second;
            return true;
        }
    }

    return false;
}

std::string StringForFeeReason(FeeReason reason)
{
    static const std::map<FeeReason, std::string> fee_reason_strings{
        {FeeReason::NONE, "None"},
        {FeeReason::HALF_ESTIMATE, "Half target estimate"},
        {FeeReason::FULL_ESTIMATE, "Full target estimate"},
        {FeeReason::DOUBLE_ESTIMATE, "Double target estimate"},
        {FeeReason::CONSERVATIVE, "Conservative estimate"},
        {FeeReason::MEMPOOL_MIN, "Mempool minimum fee"},
        {FeeReason::PAYTXFEE, "Pay transaction fee"},
        {FeeReason::FALLBACK, "Fallback fee"},
        {FeeReason::REQUIRED, "Minimum required fee"},
    };

    const auto reason_string = fee_reason_strings.find(reason);
    if (reason_string == fee_reason_strings.end()) {
        return "Unknown";
    }

    return reason_string->second;
}

std::string FeeModes(const std::string& delimiter)
{
    return Join(
        FeeModeMap(),
        delimiter,
        [](const std::pair<std::string, FeeEstimateMode>& entry) {
            return entry.first;
        });
}

std::string InvalidEstimateModeErrorMessage()
{
    return "Invalid estimate_mode parameter, must be one of: \"" +
           FeeModes("\", \"") + "\"";
}