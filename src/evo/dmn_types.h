// Copyright (c) 2023-2024 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_EVO_DMN_TYPES_H
#define SCHILLINGCOIN_EVO_DMN_TYPES_H

#include "consensus/amount.h"

#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

/**
 * SCH deterministic masternode collateral levels.
 *
 * The serialized value is zero-based:
 *
 *   Level1   = 0
 *   Level2   = 1
 *   ...
 *   Level100 = 99
 *
 * Each level represents one 100,000-SCH collateral and payment unit.
 */
enum class MnType : uint16_t {
    Level1 = 0,
    Level2 = 1,
    Level3 = 2,
    Level4 = 3,
    Level5 = 4,
    Level6 = 5,
    Level7 = 6,
    Level8 = 7,
    Level9 = 8,
    Level10 = 9,
    Level11 = 10,
    Level12 = 11,
    Level13 = 12,
    Level14 = 13,
    Level15 = 14,
    Level16 = 15,
    Level17 = 16,
    Level18 = 17,
    Level19 = 18,
    Level20 = 19,
    Level21 = 20,
    Level22 = 21,
    Level23 = 22,
    Level24 = 23,
    Level25 = 24,
    Level26 = 25,
    Level27 = 26,
    Level28 = 27,
    Level29 = 28,
    Level30 = 29,
    Level31 = 30,
    Level32 = 31,
    Level33 = 32,
    Level34 = 33,
    Level35 = 34,
    Level36 = 35,
    Level37 = 36,
    Level38 = 37,
    Level39 = 38,
    Level40 = 39,
    Level41 = 40,
    Level42 = 41,
    Level43 = 42,
    Level44 = 43,
    Level45 = 44,
    Level46 = 45,
    Level47 = 46,
    Level48 = 47,
    Level49 = 48,
    Level50 = 49,
    Level51 = 50,
    Level52 = 51,
    Level53 = 52,
    Level54 = 53,
    Level55 = 54,
    Level56 = 55,
    Level57 = 56,
    Level58 = 57,
    Level59 = 58,
    Level60 = 59,
    Level61 = 60,
    Level62 = 61,
    Level63 = 62,
    Level64 = 63,
    Level65 = 64,
    Level66 = 65,
    Level67 = 66,
    Level68 = 67,
    Level69 = 68,
    Level70 = 69,
    Level71 = 70,
    Level72 = 71,
    Level73 = 72,
    Level74 = 73,
    Level75 = 74,
    Level76 = 75,
    Level77 = 76,
    Level78 = 77,
    Level79 = 78,
    Level80 = 79,
    Level81 = 80,
    Level82 = 81,
    Level83 = 82,
    Level84 = 83,
    Level85 = 84,
    Level86 = 85,
    Level87 = 86,
    Level88 = 87,
    Level89 = 88,
    Level90 = 89,
    Level91 = 90,
    Level92 = 91,
    Level93 = 92,
    Level94 = 93,
    Level95 = 94,
    Level96 = 95,
    Level97 = 96,
    Level98 = 97,
    Level99 = 98,
    Level100 = 99,
    COUNT = 100,
    Invalid = std::numeric_limits<uint16_t>::max(),
};

template <typename T>
struct is_serializable_enum;

template <>
struct is_serializable_enum<MnType> : std::true_type {};

namespace dmn_types {

static constexpr int32_t MIN_LEVEL = 1;
static constexpr int32_t MAX_LEVEL = 100;
static constexpr CAmount COLLATERAL_UNIT = 100000 * COIN;

struct mntype_struct
{
    const int32_t level{0};
    const int32_t voting_weight{0};
    const CAmount collat_amount{0};
    const std::string_view description;
};

constexpr auto Invalid = mntype_struct{
    .level = 0,
    .voting_weight = 0,
    .collat_amount = 0,
    .description = "Invalid",
};

[[nodiscard]] constexpr int32_t GetLevel(MnType type)
{
    const auto serialized_type = static_cast<uint16_t>(type);

    if (serialized_type >= static_cast<uint16_t>(MnType::COUNT)) {
        return 0;
    }

    return static_cast<int32_t>(serialized_type) + 1;
}

[[nodiscard]] constexpr bool IsCollateralAmount(CAmount amount)
{
    if (amount < COLLATERAL_UNIT ||
        amount > COLLATERAL_UNIT * MAX_LEVEL) {
        return false;
    }

    return amount % COLLATERAL_UNIT == 0;
}

[[nodiscard]] constexpr MnType GetMnTypeByCollateralAmount(CAmount amount)
{
    if (!IsCollateralAmount(amount)) {
        return MnType::Invalid;
    }

    const auto level = static_cast<uint16_t>(amount / COLLATERAL_UNIT);

    return static_cast<MnType>(level - 1);
}

} // namespace dmn_types

[[nodiscard]] constexpr bool IsValidMnType(MnType type)
{
    return static_cast<uint16_t>(type) <
           static_cast<uint16_t>(MnType::COUNT);
}

[[nodiscard]] constexpr dmn_types::mntype_struct GetMnType(MnType type)
{
    if (!IsValidMnType(type)) {
        return dmn_types::Invalid;
    }

    const int32_t level = dmn_types::GetLevel(type);

    return dmn_types::mntype_struct{
        .level = level,
        .voting_weight = level,
        .collat_amount = level * dmn_types::COLLATERAL_UNIT,
        .description = "SCH Masternode",
    };
}

#endif // SCHILLINGCOIN_EVO_DMN_TYPES_H