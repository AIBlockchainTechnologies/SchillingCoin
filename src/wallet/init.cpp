// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2021 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "guiinterfaceutil.h"
#include "main.h"
#include "net.h"
#include "util/system.h"
#include "utilmoneystr.h"
#include "wallet/init.h"
#include "wallet/wallet.h"
#include "wallet/walletutil.h"

std::string GetWalletHelpString(bool showDebug)
{
    std::string strUsage = HelpMessageGroup("Wallet options:");
    strUsage += HelpMessageOpt("-backuppath=<dir|file>", "Specify custom backup path to add a copy of any wallet backup. If set as dir, every backup generates a timestamped file. If set as file, will rewrite to that file every backup.");
    strUsage += HelpMessageOpt("-createwalletbackups=<n>", strprintf("Number of automatic wallet backups (default: %d)", DEFAULT_CREATEWALLETBACKUPS));
    strUsage += HelpMessageOpt("-disablewallet", strprintf("Do not load the wallet and disable wallet RPC calls (default: %u)", DEFAULT_DISABLE_WALLET));
    strUsage += HelpMessageOpt("-disablesystemnotifications", "Disable OS notifications for incoming transactions");
    strUsage += HelpMessageOpt("-hdseed", "User defined seed for HD wallet (should be in hex). Only has effect during wallet creation/first start (default: randomly generated)");
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf("Set key pool size to <n> (default: %u)", DEFAULT_KEYPOOL_SIZE));
    strUsage += HelpMessageOpt("-legacywallet", "On first run, create a legacy wallet instead of an HD wallet");
    strUsage += HelpMessageOpt("-maxtxfee=<amt>", strprintf("Maximum total fees to use in a single wallet transaction, setting too low may abort large transactions (default: %s)", FormatMoney(maxTxFee)));
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf("Fees (in %s/kB) smaller than this are considered zero fee for transaction creation (default: %s)", CURRENCY_UNIT, FormatMoney(CWallet::minTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-mnemonic", "User defined mnemonic for HD wallet (BIP39). Only has effect during wallet creation/first start (default: randomly generated)");
    strUsage += HelpMessageOpt("-mnemonicpassphrase", "User defined mnemonic passphrase for HD wallet (BIP39). Only has effect during wallet creation/first start");
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf("Fee (in %s/kB) to add to transactions you send (default: %s)", CURRENCY_UNIT, FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", "Rescan the block chain for missing wallet transactions on startup");
    strUsage += HelpMessageOpt("-salvagewallet", "Attempt to recover private keys from a corrupt wallet file on startup");
    strUsage += HelpMessageOpt("-sendfreetransactions", strprintf("Send transactions as zero-fee transactions if possible (default: %u)", DEFAULT_SEND_FREE_TRANSACTIONS));
    strUsage += HelpMessageOpt("-skipmnemonicstartupui", "Skip the mnemonic startup page, which can be useful for testing");
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf("Spend unconfirmed change when sending transactions (default: %u)", DEFAULT_SPEND_ZEROCONF_CHANGE));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)", DEFAULT_TX_CONFIRM_TARGET));
    strUsage += HelpMessageOpt("-upgradewallet", "Upgrade wallet to latest format on startup");
    strUsage += HelpMessageOpt("-usehd", strprintf("Use hierarchical deterministic key generation after BIP32. Only has effect during wallet creation/first start (default: %u)", DEFAULT_USE_HD_WALLET));
    strUsage += HelpMessageOpt("-wallet=<file>", strprintf("Specify the wallet file within the data directory (default: %s). Schilling Coin supports one wallet per running instance.", DEFAULT_WALLET_DAT));
    strUsage += HelpMessageOpt("-walletdir=<dir>", "Specify directory to hold the wallet (default: <datadir>/wallets if it exists, otherwise <datadir>)");
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", "Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)");
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", "Delete all wallet transactions and recover transactions from the block chain through -rescan on startup "
        "(1 = keep transaction metadata such as account owner and payment request information, 2 = drop transaction metadata)");

    strUsage += HelpMessageGroup("Mining/Staking options:");
    strUsage += HelpMessageOpt("-coldstaking=<n>", strprintf("Enable cold staking functionality (0-1, default: %u). Disabled if staking=0", DEFAULT_COLDSTAKING));
    strUsage += HelpMessageOpt("-gen", strprintf("Generate coins (default: %u)", DEFAULT_GENERATE));
    strUsage += HelpMessageOpt("-genproclimit=<n>", strprintf("Set the number of threads for coin generation if enabled (-1 = all cores, default: %d)", DEFAULT_GENERATE_PROCLIMIT));
    strUsage += HelpMessageOpt("-minstakesplit=<amt>", strprintf("Minimum positive amount (in SCH) allowed by GUI and RPC for the stake split threshold (default: %s)", FormatMoney(DEFAULT_MIN_STAKE_SPLIT_THRESHOLD)));
    strUsage += HelpMessageOpt("-reservebalance=<amt>", "Keep the specified amount available for spending at all times (default: 0)");
    strUsage += HelpMessageOpt("-SCHstake=<n>", strprintf("Enable or disable staking functionality for SCH inputs (0-1, default: %u)", DEFAULT_STAKING));
    strUsage += HelpMessageOpt("-staking=<n>", strprintf("Enable staking functionality (0-1, default: %u)", DEFAULT_STAKING));

    if (showDebug) {
        strUsage += HelpMessageGroup("Wallet debugging/testing options:");
        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf("Flush database activity from memory pool to disk log every <n> megabytes (default: %u)", DEFAULT_WALLET_DBLOGSIZE));
        strUsage += HelpMessageOpt("-flushwallet", strprintf("Run a thread to flush wallet periodically (default: %u)", DEFAULT_FLUSHWALLET));
        strUsage += HelpMessageOpt("-printcoinstake", "Display verbose coin stake messages in the debug log");
        strUsage += HelpMessageOpt("-printstakemodifier", "Display stake modifier calculations in the debug log");
        strUsage += HelpMessageOpt("-privdb", strprintf("Set the DB_PRIVATE flag in the wallet database environment (default: %u)", DEFAULT_WALLET_PRIVDB));
    }

    return strUsage;
}

bool WalletParameterInteraction()
{
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        return true;
    }

    if (gArgs.GetBoolArg("-sysperms", false)) {
        return UIError(strprintf(
            _("%s is not allowed in combination with enabled wallet functionality"),
            "-sysperms"));
    }

    gArgs.SoftSetArg("-wallet", DEFAULT_WALLET_DAT);

    if (gArgs.GetArgs("-wallet").size() > 1) {
        return UIError(_(
            "Multiple -wallet arguments are not supported. "
            "Schilling Coin supports one wallet per running instance."));
    }

    if (gArgs.GetBoolArg("-salvagewallet", false)) {
        // Rewrite just private keys: rescan to find transactions.
        if (gArgs.SoftSetBoolArg("-rescan", true)) {
            LogPrintf(
                "%s: parameter interaction: -salvagewallet=1 -> setting -rescan=1\n",
                __func__);
        }
    }

    const bool zapWalletTxes =
        gArgs.GetBoolArg("-zapwallettxes", false);

    // -zapwallettxes implies dropping the mempool on startup.
    if (zapWalletTxes &&
        gArgs.SoftSetBoolArg("-persistmempool", false)) {
        LogPrintf(
            "%s: parameter interaction: -zapwallettxes=<mode> -> setting -persistmempool=0\n",
            __func__);
    }

    // -zapwallettxes implies a rescan.
    if (zapWalletTxes &&
        gArgs.SoftSetBoolArg("-rescan", true)) {
        LogPrintf(
            "%s: parameter interaction: -zapwallettxes=<mode> -> setting -rescan=1\n",
            __func__);
    }

    if (gArgs.IsArgSet("-mintxfee")) {
        const std::string minTxFeeArg =
            gArgs.GetArg("-mintxfee", "");

        CAmount amount = 0;
        if (ParseMoney(minTxFeeArg, amount) && amount > 0) {
            CWallet::minTxFee = CFeeRate(amount);
        } else {
            return UIError(
                AmountErrMsg("mintxfee", minTxFeeArg));
        }
    }

    if (gArgs.IsArgSet("-paytxfee")) {
        const std::string payTxFeeArg =
            gArgs.GetArg("-paytxfee", "");

        CAmount feePerK = 0;
        if (!ParseMoney(payTxFeeArg, feePerK)) {
            return UIError(
                AmountErrMsg("paytxfee", payTxFeeArg));
        }

        if (feePerK > nHighTransactionFeeWarning) {
            UIWarning(_(
                "Warning: -paytxfee is set very high! "
                "This is the transaction fee you will pay if you send a transaction."));
        }

        payTxFee = CFeeRate(feePerK, 1000);

        if (payTxFee < ::minRelayTxFee) {
            return UIError(strprintf(
                _("Invalid amount for -paytxfee=<amount>: '%s' "
                  "(must be at least %s)"),
                payTxFeeArg,
                ::minRelayTxFee.ToString()));
        }
    }

    if (gArgs.IsArgSet("-maxtxfee")) {
        const std::string maxTxFeeArg =
            gArgs.GetArg("-maxtxfee", "");

        CAmount maxFee = 0;
        if (!ParseMoney(maxTxFeeArg, maxFee)) {
            return UIError(
                AmountErrMsg("maxtxfee", maxTxFeeArg));
        }

        if (maxFee > nHighTransactionMaxFeeWarning) {
            UIWarning(_(
                "Warning: -maxtxfee is set very high! "
                "Fees this large could be paid on a single transaction."));
        }

        maxTxFee = maxFee;

        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee) {
            return UIError(strprintf(
                _("Invalid amount for -maxtxfee=<amount>: '%s' "
                  "(must be at least the minimum relay fee of %s "
                  "to prevent stuck transactions)"),
                maxTxFeeArg,
                ::minRelayTxFee.ToString()));
        }
    }

    if (gArgs.IsArgSet("-minstakesplit")) {
        const std::string minStakeSplitArg =
            gArgs.GetArg("-minstakesplit", "");

        CAmount amount = 0;
        if (ParseMoney(minStakeSplitArg, amount) && amount > 0) {
            CWallet::minStakeSplitThreshold = amount;
        } else {
            return UIError(
                AmountErrMsg("minstakesplit", minStakeSplitArg));
        }
    }

    nTxConfirmTarget = gArgs.GetArg("-txconfirmtarget", DEFAULT_TX_CONFIRM_TARGET);

    bSpendZeroConfChange = gArgs.GetBoolArg("-spendzeroconfchange", DEFAULT_SPEND_ZEROCONF_CHANGE);

    bdisableSystemnotifications = gArgs.GetBoolArg("-disablesystemnotifications", false);

    fSendFreeTransactions = gArgs.GetBoolArg("-sendfreetransactions", DEFAULT_SEND_FREE_TRANSACTIONS);

    return true;
}

bool WalletVerify()
{
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        return true;
    }

    if (gArgs.IsArgSet("-walletdir")) {
        fs::path wallet_dir = gArgs.GetArg("-walletdir", "");
        if (!fs::exists(wallet_dir)) {
            return UIError(strprintf(_("Specified -walletdir \"%s\" does not exist"), wallet_dir.string()));
        } else if (!fs::is_directory(wallet_dir)) {
            return UIError(strprintf(_("Specified -walletdir \"%s\" is not a directory"), wallet_dir.string()));
        } else if (!wallet_dir.is_absolute()) {
            return UIError(strprintf(_("Specified -walletdir \"%s\" is a relative path"), wallet_dir.string()));
        }
    }

    LogPrintf("Using wallet directory %s\n", GetWalletDir().string());

    uiInterface.InitMessage(_("Verifying wallet(s)..."));

    // Keep track of each wallet absolute path to detect duplicates.
    std::set<fs::path> wallet_paths;

    for (const std::string& walletFile : gArgs.GetArgs("-wallet")) {
        auto opRes = VerifyWalletPath(walletFile);
        if (!opRes) return UIError(opRes.getError());

        fs::path wallet_path = fs::absolute(walletFile, GetWalletDir());
        if (!wallet_paths.insert(wallet_path).second) {
            return UIError(strprintf(_("Error loading wallet %s. Duplicate %s filename specified."), walletFile, "-wallet"));
        }

        std::string strError;
        if (!WalletBatch::VerifyEnvironment(wallet_path, strError)) {
            return UIError(strError);
        }

        if (gArgs.GetBoolArg("-salvagewallet", false)) {
            // Recover readable keypairs:
            CWallet dummyWallet("dummy", WalletDatabase::CreateDummy());
            std::string backup_filename;
            // Even if we don't use this lock in this function, we want to preserve
            // lock order in LoadToWallet if query of chain state is needed to know
            // tx status. If lock can't be taken, tx confirmation status may be not
            // reliable.
            LOCK(cs_main);
            if (!WalletBatch::Recover(wallet_path, (void *)&dummyWallet, WalletBatch::RecoverKeysOnlyFilter, backup_filename)) {
                return false;
            }
        }

        std::string strWarning;
        bool dbV = WalletBatch::VerifyDatabaseFile(wallet_path, strWarning, strError);
        if (!strWarning.empty()) {
            UIWarning(strWarning);
        }
        if (!dbV) {
            return UIError(strError);
        }
    }

    return true;
}

bool InitLoadWallet(const std::vector<std::string>& words)
{
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        pwalletMain = nullptr;
        LogPrintf("Wallet disabled!\n");
        return true;
    }

    const std::string walletFile =
        gArgs.GetArg("-wallet", DEFAULT_WALLET_DAT);

    const fs::path walletPath =
        fs::absolute(walletFile, GetWalletDir());

    CWallet* const pwallet =
        words.empty()
            ? CWallet::CreateWalletFromFile(walletFile, walletPath)
            : CWallet::RestoreWalletFromMnemonic(walletFile, walletPath, words);

    if (!pwallet) {
        return false;
    }

    pwalletMain = pwallet;

    std::string strWarning;
    std::string strError;

    if (!AutoBackupWallet(*pwalletMain, strWarning, strError)) {
        if (!strWarning.empty()) {
            UIWarning(strprintf(
                "%s: %s",
                walletFile,
                strWarning));
        }

        if (!strError.empty()) {
            return UIError(strprintf(
                "%s: %s",
                walletFile,
                strError));
        }
    }

    return true;
}