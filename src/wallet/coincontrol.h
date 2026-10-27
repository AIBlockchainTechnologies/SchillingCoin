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

/** Coin Control Features. */
class CCoinControl
{
public:
    CTxDestination destChange = CNoDestination();
    bool fSplitBlock;
    int nSplitBlock;
    //! If false, allows unselected inputs, but requires all selected inputs be used
    bool fAllowOtherInputs;
    //! Includes watch only addresses which match the ISMINE_WATCH_SOLVABLE criteria
    bool fAllowWatchOnly;
    //! Minimum absolute fee (not per kilobyte)
    CAmount nMinimumTotalFee;
    //! Override automatic minimum and maximum fee checks
    bool fOverrideFeeRate;
    //! Override the wallet's selected fee rate when set
    Optional<CFeeRate> m_feerate;
    //! Override the wallet's default confirmation target when set
    Optional<unsigned int> m_confirm_target;

    CCoinControl()
    {
        SetNull();
    }

    void SetNull()
    {
        destChange = CNoDestination();
        setSelected.clear();
        fAllowOtherInputs = false;
        fAllowWatchOnly = true;
        nMinimumTotalFee = 0;
        fOverrideFeeRate = false;
        m_feerate = nullopt;
        m_confirm_target = nullopt;
        fSplitBlock = false;
        nSplitBlock = 1;
    }

    bool HasSelected() const
    {
        return (!setSelected.empty());
    }

    bool IsSelected(const COutPoint& output) const
    {
        return setSelected.count(output) > 0;
    }

    bool IsSelected(const uint256& hash, unsigned int n) const
    {
        return IsSelected(COutPoint(hash, n));
    }

    void Select(const COutPoint& output)
    {
        setSelected.insert(output);
    }

    void UnSelect(const COutPoint& output)
    {
        setSelected.erase(output);
    }

    void UnSelectAll()
    {
        setSelected.clear();
    }

    void ListSelected(std::vector<COutPoint>& vOutpoints) const
    {
        vOutpoints.assign(setSelected.begin(), setSelected.end());
    }

    unsigned int QuantitySelected() const
    {
        return setSelected.size();
    }

    void SetSelection(const std::set<COutPoint>& selected)
    {
        setSelected = selected;
    }

private:
    std::set<COutPoint> setSelected;
};

#endif // SCHILLINGCOIN_WALLET_COINCONTROL_H