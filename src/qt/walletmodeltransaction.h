// Copyright (c) 2011-2013 The Bitcoin developers
// copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETMODELTRANSACTION_H
#define BITCOIN_QT_WALLETMODELTRANSACTION_H

#include "walletmodel.h"

#include <QObject>
#include <vector>

class SendCoinsRecipient;
class CReserveKey;
class CWallet;
class CWalletTx;

/** Data model for a walletmodel transaction. */
class WalletModelTransaction
{
public:
    explicit WalletModelTransaction(const std::vector<SendCoinsRecipient>& recipients);
    ~WalletModelTransaction();

    std::vector<SendCoinsRecipient> getRecipients();

    CWalletTx* getTransaction();
    unsigned int getTransactionSize();

    void setTransactionFee(const CAmount& newFee);
    CAmount getTransactionFee();

    CAmount getTotalTransactionAmount();

    void newPossibleKeyChange(CWallet* wallet);
    CReserveKey* getPossibleKeyChange();

private:
    const std::vector<SendCoinsRecipient> recipients;
    CWalletTx* walletTransaction;
    CReserveKey* keyChange;
    CAmount fee;
};

#endif // BITCOIN_QT_WALLETMODELTRANSACTION_H