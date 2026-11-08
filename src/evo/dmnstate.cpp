// Copyright (c) 2018-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/dmnstate.h>

#include <evo/netinfo.h>
#include <script/standard.h>
#include <univalue.h>

std::string CDeterministicMNState::ToString() const
{
    CTxDestination dest;
    std::string payoutAddress = "unknown";
    std::string operatorPayoutAddress = "none";
    if (ExtractDestination(scriptPayout, dest)) {
        payoutAddress = EncodeDestination(dest);
    }
    const std::string payoutList = IsShared() ? PayoutListToString(shares, nEarlyPeriodBlocks, nEarlyPenalty)
                                            : PayoutListToString(GetOwnerPayouts(*this));
    if (ExtractDestination(scriptOperatorPayout, dest)) {
        operatorPayoutAddress = EncodeDestination(dest);
    }

    return strprintf("CDeterministicMNState(nVersion=%d, nRegisteredHeight=%d, nLastPaidHeight=%d, nPoSePenalty=%d, "
                     "nPoSeRevivedHeight=%d, nPoSeBanHeight=%d, nRevocationReason=%d, "
                     "ownerAddress=%s, pubKeyOperator=%s, votingAddress=%s, netInfo=%s, payoutAddress=%s, payouts=%s, "
                     "operatorPayoutAddress=%s)\n",
                     nVersion, nRegisteredHeight, nLastPaidHeight, nPoSePenalty, nPoSeRevivedHeight, nPoSeBanHeight,
                     nRevocationReason, EncodeDestination(PKHash(keyIDOwner)), pubKeyOperator.ToString(),
                     EncodeDestination(PKHash(keyIDVoting)), netInfo->ToString(), payoutAddress, payoutList, operatorPayoutAddress);
}

std::vector<CScript> CDeterministicMNState::GetOwnerRewardScripts() const
{
    std::vector<CScript> ret;
    if (IsShared()) {
        ret.reserve(shares.size());
        for (const auto& share : shares) {
            ret.emplace_back(share.RewardScript());
        }
    } else {
        for (const auto& payout : GetOwnerPayouts(*this)) {
            ret.emplace_back(payout.scriptPayout);
        }
    }
    return ret;
}

UniValue CDeterministicMNStateDiff::ToJson(MnType nType) const
{
    UniValue obj(UniValue::VOBJ);
    if (fields & Field_nVersion) {
        obj.pushKV("version", state.nVersion);
    }
    if (fields & Field_netInfo) {
        if (IsServiceDeprecatedRPCEnabled()) {
            obj.pushKV("service", state.netInfo->GetPrimary().ToStringAddrPort());
        }
    }
    if (fields & Field_nRegisteredHeight) {
        obj.pushKV("registeredHeight", state.nRegisteredHeight);
    }
    if (fields & Field_nLastPaidHeight) {
        obj.pushKV("lastPaidHeight", state.nLastPaidHeight);
    }
    if (fields & Field_nConsecutivePayments) {
        obj.pushKV("consecutivePayments", state.nConsecutivePayments);
    }
    if (fields & Field_nPoSePenalty) {
        obj.pushKV("PoSePenalty", state.nPoSePenalty);
    }
    if (fields & Field_nPoSeRevivedHeight) {
        obj.pushKV("PoSeRevivedHeight", state.nPoSeRevivedHeight);
    }
    if (fields & Field_nPoSeBanHeight) {
        obj.pushKV("PoSeBanHeight", state.nPoSeBanHeight);
    }
    if (fields & Field_nRevocationReason) {
        obj.pushKV("revocationReason", state.nRevocationReason);
    }
    if (fields & Field_keyIDOwner) {
        obj.pushKV("ownerAddress", EncodeDestination(PKHash(state.keyIDOwner)));
    }
    if (fields & Field_keyIDVoting) {
        obj.pushKV("votingAddress", EncodeDestination(PKHash(state.keyIDVoting)));
    }
    if (fields & Field_scriptPayout) {
        CTxDestination dest;
        if (ExtractDestination(state.scriptPayout, dest)) {
            obj.pushKV("payoutAddress", EncodeDestination(dest));
        }
    }
    if (fields & Field_payouts) {
        obj.pushKV("payouts", PayoutListToJson(state.payouts));
    }
    if (fields & Field_shares) {
        obj.pushKV("shares", ShareListToJson(state.shares));
    }
    if (fields & Field_nEarlyPeriodBlocks) {
        obj.pushKV("earlyPeriodBlocks", static_cast<int64_t>(state.nEarlyPeriodBlocks));
    }
    if (fields & Field_nEarlyPenalty) {
        obj.pushKV("earlyPenalty", state.nEarlyPenalty);
    }
    if (fields & Field_scriptOperatorPayout) {
        CTxDestination dest;
        if (ExtractDestination(state.scriptOperatorPayout, dest)) {
            obj.pushKV("operatorPayoutAddress", EncodeDestination(dest));
        }
    }
    if (fields & Field_pubKeyOperator) {
        obj.pushKV("pubKeyOperator", state.pubKeyOperator.ToString());
    }
    {
        const bool has_netinfo = (fields & Field_netInfo);

        UniValue netInfoObj(UniValue::VOBJ);
        if (has_netinfo) {
            netInfoObj = state.netInfo->ToJson();
        }
        if (!netInfoObj.empty()) {
            obj.pushKV("addresses", netInfoObj);
        }
    }
    return obj;
}