// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Money parsing/formatting utilities.
 */
#ifndef SCHILLINGCOIN_UTILMONEYSTR_H
#define SCHILLINGCOIN_UTILMONEYSTR_H

#include <stdint.h>
#include <string>

#include "consensus/amount.h"

std::string FormatMoney(const CAmount& n, bool fPlus = false);
bool ParseMoney(const std::string& str, CAmount& nRet);
bool ParseMoney(const char* pszIn, CAmount& nRet);

#endif // SCHILLINGCOIN_UTILMONEYSTR_H