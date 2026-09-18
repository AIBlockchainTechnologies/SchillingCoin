// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SEND_H
#define SEND_H

#include <QWidget>
#include <QPushButton>

#include "qt/schillingcoin/pwidget.h"
#include "qt/schillingcoin/contactsdropdown.h"
#include "qt/schillingcoin/sendmultirow.h"
#include "qt/schillingcoin/sendcustomfeedialog.h"
#include "walletmodel.h"
#include "coincontroldialog.h"
#include "qt/schillingcoin/tooltipmenu.h"

#include <vector>

static const int MAX_SEND_POPUP_ENTRIES = 8;

class SchillingCoinGUI;
class ClientModel;
class WalletModel;
class WalletModelTransaction;

namespace Ui {
class send;
class QPushButton;
}

class SendWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SendWidget(SchillingCoinGUI* parent);
    ~SendWidget();

    void addEntry();

    void loadClientModel() override;
    void loadWalletModel() override;

Q_SIGNALS:
    void receivedURI(const QString& uri);

public Q_SLOTS:
    void onChangeAddressClicked();
    void onChangeCustomFeeClicked();
    void onCoinControlClicked();
    void onOpenUriClicked();
    void onValueChanged();
    void refreshAmounts();
    void changeTheme(bool isLightTheme, QString &theme) override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private Q_SLOTS:
    void onSCHSelected(bool _isSCH);
    void onSendClicked();
    void onContactsClicked(SendMultiRow* entry);
    void onMenuClicked(SendMultiRow* entry);
    void onAddEntryClicked();
    void clearEntries();
    void clearAll();
    void refreshView();
    void onContactMultiClicked();
    void onDeleteClicked();
    void onResetCustomOptions(bool fRefreshAmounts);

private:
    Ui::send *ui;
    QPushButton *coinIcon;
    QPushButton *btnContacts;

    SendCustomFeeDialog* customFeeDialog = nullptr;
    bool isCustomFeeSelected = false;

    int nDisplayUnit;

    // QList → std::vector modernization
    std::vector<SendMultiRow*> entries;

    CoinControlDialog *coinControlDialog = nullptr;

    ContactsDropdown *menuContacts = nullptr;
    TooltipMenu *menu = nullptr;

    SendMultiRow* focusedEntry = nullptr;

    bool isSCH = true;
    void resizeMenu();

    // QList → std::vector modernization
    QString recipientsToString(const std::vector<SendCoinsRecipient>& recipients);
    SendMultiRow* createEntry();
    bool send(const std::vector<SendCoinsRecipient>& recipients);
    void updateEntryLabels(const std::vector<SendCoinsRecipient>& recipients);
};

#endif // SEND_H