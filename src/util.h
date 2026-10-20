// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Server/client environment: argument handling, config file parsing,
 * logging, thread wrappers
 */
#ifndef SCHILLINGCOIN_UTIL_H
#define SCHILLINGCOIN_UTIL_H

#if defined(HAVE_CONFIG_H)
#include "config/schillingcoin-config.h"
#endif

#include "logging.h"
#include "compat.h"
#include "fs.h"
#include "tinyformat.h"
#include "utiltime.h"
#include "util/threadnames.h"

#include <atomic>
#include <exception>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/thread/exceptions.hpp>
#include <boost/thread/condition_variable.hpp> // for boost::thread_interrupted

// Debugging macros
// Uncomment the following line to enable debugging messages
// or enable on a per file basis prior to inclusion of util.h
//#define ENABLE_SchillingCoin_DEBUG
#ifdef ENABLE_SchillingCoin_DEBUG
#define DBG( x ) x
#else
#define DBG( x )
#endif

// SchillingCoin only features
extern std::atomic<bool> fMasterNode;
extern bool fLiteMode;
extern int64_t enforceMasternodePaymentsTime;
extern std::string strMasterNodeAddr;
extern std::string strMasterNodePrivKey;
extern int keysLoaded;
extern bool fSucessfullyLoaded;

extern std::map<std::string, std::string> mapArgs;
extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern bool fDebug;
extern std::string strMiscWarning;
extern bool fLogIPs;

double double_safe_addition(double fValue, double fIncrement);
double double_safe_multiplication(double fValue, double fmultiplicator);
void ParseParameters(int argc, const char* const argv[]);

/**
 * Legacy SCH compatibility helper.
 */
bool TryCreateDirectory(const fs::path& p);

#ifndef WIN32
#endif

fs::path GetTempPath();

/**
 * Return string argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault);

/**
 * Set an argument if it doesn't already have a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);

#endif // SCHILLINGCOIN_UTIL_H