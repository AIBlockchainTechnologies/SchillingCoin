// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2022 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

/**
 * Server/client environment: argument handling, config file parsing,
 * thread wrappers
 */
#ifndef SCHILLINGCOIN_UTIL_SYSTEM_H
#define SCHILLINGCOIN_UTIL_SYSTEM_H

#if defined(HAVE_CONFIG_H)
#include "config/schillingcoin-config.h"
#endif

#include "compat.h"
#include <common/args.h>
#include "logging.h"
#include "optional.h"
#include "sync.h"
#include "tinyformat.h"
#include "util/fs.h"
#include "util/threadnames.h"
#include "util/time.h"

#include <atomic>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/signals2/signal.hpp>
#include <boost/thread/condition_variable.hpp>

extern const char * const SCHILLINGCOIN_CONF_FILENAME;
extern const char * const SCHILLINGCOIN_MASTERNODE_CONF_FILENAME;
extern const char * const DEFAULT_DEBUGLOGFILE;

// SCHILLINGCOIN-only features
extern std::atomic<bool> fMasterNode;

void SetupEnvironment();
bool SetupNetworking();

template<typename... Args>
bool error(const char* fmt, const Args&... args)
{
    LogPrintf("ERROR: %s\n", tfm::format(fmt, args...));
    return false;
}

// The blocks directory is always net specific.
const fs::path &GetBlocksDir();
const fs::path &GetDataDir(bool fNetSpecific = true);
// Return true if -datadir option points to a valid directory or is not specified.
bool CheckDataDirOption();
void ClearDatadirCache();
fs::path GetConfigFile(const std::string& confPath);
fs::path GetMasternodeConfigFile();
#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif

void runCommand(std::string strCommand);

/**
 * Format a string to be used as option description in help messages
 *
 * @param option Option message (e.g. "-rpcuser=<user>")
 * @param message Option description (e.g. "Username for JSON-RPC connections")
 * @return the formatted string
 */
std::string HelpMessageOpt(const std::string& option, const std::string& message);

/**
 * Return the number of cores available on the current system.
 * @note This does count virtual cores, such as those provided by HyperThreading.
 */
int GetNumCores();

void SetThreadPriority(int nPriority);

fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific = true);

/**
 * On platforms that support it, tell the kernel the calling thread is
 * CPU-intensive and non-interactive. See SCHED_BATCH in sched(7) for details.
 *
 * @return The return value of sched_setschedule(), or 1 on systems without
 * sched_setchedule().
 */
int ScheduleBatchPriority(void);

namespace util {

#ifdef WIN32
class WinCmdLineArgs
{
public:
    WinCmdLineArgs();
    ~WinCmdLineArgs();
    std::pair<int, char**> get();

private:
    int argc;
    char** argv;
    std::vector<std::string> args;
};
#endif

} // namespace util

#endif // SCHILLINGCOIN_UTIL_SYSTEM_H