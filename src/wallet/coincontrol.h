// Copyright (c) 2011-2013 The Bitcoin developers
// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_WALLET_COINCONTROL_H
#define SCHILLINGCOIN_WALLET_COINCONTROL_H

#include "optional.h"
#include "policy/feerate.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "util/fees.h"

#include <set>
#include <vector>

static constexpr bool DEFAULT_AVOIDPARTIALSPENDS = false;
static constexpr int DEFAULT_MIN_DEPTH = 0;
static constexpr int DEFAULT_MAX_DEPTH = 9999999;

/** Coin Control Features. */
class CCoinControl
{
public:
    //! Custom change destination, if not set a new destination is generated.
    CTxDestination destChange = CNoDestination();

    //! SCH split-block controls.
    bool fSplitBlock;
    int nSplitBlock;

    //! Allow the selection process to add unselected wallet inputs.
    bool fAllowOtherInputs;

    //! Require every selected input to be used.
    bool fRequireAllInputs;

    //! Include watch-only addresses that are solvable.
    bool fAllowWatchOnly;

    //! Minimum absolute transaction fee, not a rate.
    CAmount nMinimumTotalFee;

    //! Override automatic minimum and maximum fee checks.
    bool fOverrideFeeRate;

    //! Override the wallet-selected transaction fee rate.
    Optional<CFeeRate> m_feerate;

    //! Override the discard fee rate used during transaction creation.
    Optional<CFeeRate> m_discard_feerate;

    //! Override the wallet's default confirmation target.
    Optional<unsigned int> m_confirm_target;

    //! Avoid partial use of funds sent to the same address.
    bool m_avoid_partial_spends;

    //! Avoid spending outputs associated with previously used addresses.
    bool m_avoid_address_reuse;

    //! Select economical, conservative, or default fee estimation.
    FeeEstimateMode m_fee_mode;

    //! Minimum chain depth for available coins.
    int m_min_depth;

    //! Maximum chain depth for available coins.
    int m_max_depth;

    CCoinControl();

    void SetNull();

    bool HasSelected() const;

    bool IsSelected(const COutPoint& output) const;

    bool IsSelected(const uint256& hash, unsigned int n) const
    {
        return IsSelected(COutPoint(hash, n));
    }

    void Select(const COutPoint& output);

    void UnSelect(const COutPoint& output);

    void UnSelectAll();

    void ListSelected(std::vector<COutPoint>& outpoints) const;

    unsigned int QuantitySelected() const;

    void SetSelection(const std::set<COutPoint>& selected);

private:
    std::set<COutPoint> m_selected;
};

#endif // SCHILLINGCOIN_WALLET_COINCONTROL_H