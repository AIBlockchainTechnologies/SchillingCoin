// Copyright (c) 2018-2021 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/coincontrol.h"

#include "util/system.h"

CCoinControl::CCoinControl()
{
    SetNull();
}

void CCoinControl::SetNull()
{
    destChange = CNoDestination();

    m_selected.clear();

    fSplitBlock = false;
    nSplitBlock = 1;

    fAllowOtherInputs = false;
    fRequireAllInputs = true;
    fAllowWatchOnly = false;

    nMinimumTotalFee = 0;
    fOverrideFeeRate = false;

    m_feerate = nullopt;
    m_discard_feerate = nullopt;
    m_confirm_target = nullopt;

    m_avoid_partial_spends =
        gArgs.GetBoolArg("-avoidpartialspends", DEFAULT_AVOIDPARTIALSPENDS);
    m_avoid_address_reuse = false;

    m_fee_mode = FeeEstimateMode::UNSET;

    m_min_depth = DEFAULT_MIN_DEPTH;
    m_max_depth = DEFAULT_MAX_DEPTH;
}

bool CCoinControl::HasSelected() const
{
    return !m_selected.empty();
}

bool CCoinControl::IsSelected(const COutPoint& output) const
{
    return m_selected.count(output) > 0;
}

void CCoinControl::Select(const COutPoint& output)
{
    m_selected.insert(output);
}

void CCoinControl::UnSelect(const COutPoint& output)
{
    m_selected.erase(output);
}

void CCoinControl::UnSelectAll()
{
    m_selected.clear();
}

void CCoinControl::ListSelected(std::vector<COutPoint>& outpoints) const
{
    outpoints.assign(m_selected.begin(), m_selected.end());
}

unsigned int CCoinControl::QuantitySelected() const
{
    return m_selected.size();
}

void CCoinControl::SetSelection(const std::set<COutPoint>& selected)
{
    m_selected = selected;
}
