// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_QT_OPENURIDIALOG_H
#define SCHILLINGCOIN_QT_OPENURIDIALOG_H

#include <QDialog>
#include "qt/schillingcoin/snackbar.h"

namespace Ui
{
class OpenURIDialog;
}

class OpenURIDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenURIDialog(QWidget* parent);
    ~OpenURIDialog();

    QString getURI();
    void showEvent(QShowEvent *event) override;

protected Q_SLOTS:
    void accept() override;

private Q_SLOTS:
    void on_selectFileButton_clicked();

private:
    Ui::OpenURIDialog* ui;
    SnackBar *snackBar = nullptr;
    void inform(const QString& str);
};

#endif // SCHILLINGCOIN_QT_OPENURIDIALOG_H
