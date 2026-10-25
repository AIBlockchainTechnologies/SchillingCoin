// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/schillingcoin/sendcustomfeedialog.h"
#include "qt/schillingcoin/forms/ui_sendcustomfeedialog.h"

#include "guiutil.h"
#include "optionsmodel.h"
#include "qt/schillingcoin/qtutils.h"
#include "walletmodel.h"

#include <QComboBox>
#include <QListView>

SendCustomFeeDialog::SendCustomFeeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCustomFeeDialog)
{
    ui->setupUi(this);

    // Stylesheet
    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");

    // Text
    ui->labelTitle->setText(tr("Customize Fee"));
    ui->labelMessage->setText(tr("Customize the transaction fee, depending on the fee value your transaction might be included faster in the blockchain."));
    setCssProperty(ui->labelTitle, "text-title-dialog");
    setCssProperty(ui->labelMessage, "text-main-grey");

    // Recommended
    setCssProperty(ui->labelFee, "text-main-grey-big");
    setCssProperty(ui->comboBoxRecommended, "btn-combo-dialog");
    ui->comboBoxRecommended->setView(new QListView());
    ui->comboBoxRecommended->addItem(tr("Normal"), 5);
    ui->comboBoxRecommended->addItem(tr("Slow"), 20);
    ui->comboBoxRecommended->addItem(tr("Fast"), 1);

    // Custom
    setCssProperty(ui->labelCustomFee, "label-subtitle-dialog");
    ui->lineEditCustomFee->setPlaceholderText("0.000001 SCH");
    initCssEditLine(ui->lineEditCustomFee, true);
    GUIUtil::setupAmountWidget(ui->lineEditCustomFee, this);

    // Buttons
    setCssProperty(ui->btnEsc, "ic-close");
    setCssProperty(ui->btnCancel, "btn-dialog-cancel");
    ui->btnSave->setText(tr("SAVE"));
    setCssBtnPrimary(ui->btnSave);

    connect(ui->btnEsc, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnSave, SIGNAL(clicked()), this, SLOT(accept()));
    connect(ui->checkBoxCustom, SIGNAL(clicked()), this, SLOT(onCustomChecked()));
    connect(ui->checkBoxRecommended, SIGNAL(clicked()), this, SLOT(onRecommendedChecked()));
    connect(ui->comboBoxRecommended, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(updateFee()));
    if(parent) connect(parent, SIGNAL(themeChanged(bool, QString&)), this, SLOT(onChangeTheme(bool, QString&)));
    ui->checkBoxRecommended->setChecked(true);
}

void SendCustomFeeDialog::setWalletModel(WalletModel* walletModel){
    this->walletModel = walletModel;
}

void SendCustomFeeDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    updateFee();

    ui->labelCustomFee->setText(
        BitcoinUnits::name(walletModel->getOptionsModel()->getDisplayUnit()) + "/kB");

    if (walletModel->hasWalletCustomFee()) {
        ui->checkBoxCustom->setChecked(true);
        onCustomChecked();
    } else {
        ui->checkBoxRecommended->setChecked(true);
        onRecommendedChecked();
    }
}

void SendCustomFeeDialog::onCustomChecked()
{
    const bool isChecked = ui->checkBoxCustom->checkState() == Qt::Checked;

    ui->lineEditCustomFee->setEnabled(isChecked);
    ui->comboBoxRecommended->setEnabled(!isChecked);
    ui->checkBoxRecommended->setChecked(!isChecked);

    if (isChecked) {
        CAmount nFee = walletModel->getNetMinFee();
        walletModel->getWalletCustomFee(nFee);

        ui->lineEditCustomFee->setText(
            BitcoinUnits::format(
                walletModel->getOptionsModel()->getDisplayUnit(),
                nFee));
    } else {
        ui->lineEditCustomFee->clear();
    }
}

void SendCustomFeeDialog::onRecommendedChecked()
{
    const bool isChecked = ui->checkBoxRecommended->checkState() == Qt::Checked;

    ui->lineEditCustomFee->setEnabled(!isChecked);
    ui->comboBoxRecommended->setEnabled(isChecked);
    ui->checkBoxCustom->setChecked(!isChecked);

    if (isChecked) {
        ui->lineEditCustomFee->clear();
    }
}

// Fast = 1.
// Medium = 5
// Slow = 20
void SendCustomFeeDialog::updateFee()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    const QVariant num = ui->comboBoxRecommended->currentData();

    bool res = false;
    const int nBlocksToConfirm = num.toInt(&res);

    if (res) {
        feeRate = mempool.estimateFee(nBlocksToConfirm);

        if (feeRate < CWallet::minTxFee)
            feeRate = CWallet::minTxFee;

        ui->labelFee->setText(
            BitcoinUnits::formatWithUnit(
                walletModel->getOptionsModel()->getDisplayUnit(),
                feeRate.GetFeePerK()) + "/kB");
    }
}

void SendCustomFeeDialog::accept()
{
    const bool fUseCustomFee =
        ui->checkBoxCustom->checkState() == Qt::Checked;

    const CAmount customFee = getFeeRate().GetFeePerK();
    const CAmount netMinFee = walletModel->getNetMinFee();

    if (fUseCustomFee && customFee <= 0) {
        inform(tr("Invalid custom fee amount"));
        return;
    }

    const CAmount insaneFee =
        ::minRelayTxFee.GetFeePerK() * 10000;

    if (customFee >= insaneFee) {
        ui->lineEditCustomFee->setText(
            BitcoinUnits::format(
                walletModel->getOptionsModel()->getDisplayUnit(),
                insaneFee - netMinFee));

        inform(
            tr("Fee too high. Must be below: %1")
                .arg(BitcoinUnits::formatWithUnit(
                    walletModel->getOptionsModel()->getDisplayUnit(),
                    insaneFee)));

        return;
    }

    if (customFee < netMinFee) {
        CAmount nFee = netMinFee;

        if (walletModel->hasWalletCustomFee())
            walletModel->getWalletCustomFee(nFee);

        ui->lineEditCustomFee->setText(
            BitcoinUnits::format(
                walletModel->getOptionsModel()->getDisplayUnit(),
                nFee));

        inform(
            tr("Fee too low. Must be at least: %1")
                .arg(BitcoinUnits::formatWithUnit(
                    walletModel->getOptionsModel()->getDisplayUnit(),
                    netMinFee)));

        return;
    }

    walletModel->setWalletCustomFee(fUseCustomFee, customFee);
    QDialog::accept();
}

void SendCustomFeeDialog::clear()
{
    ui->comboBoxRecommended->setCurrentIndex(0);
}

CFeeRate SendCustomFeeDialog::getFeeRate()
{
    if (ui->checkBoxRecommended->isChecked()) {
        return feeRate;
    }

    const CAmount value = GUIUtil::parseValue(
        ui->lineEditCustomFee->text(),
        walletModel->getOptionsModel()->getDisplayUnit());

    return CFeeRate(value);
}

bool SendCustomFeeDialog::isCustomFeeChecked()
{
    return ui->checkBoxCustom->checkState() == Qt::Checked;
}

void SendCustomFeeDialog::onChangeTheme(bool isLightTheme, QString& theme)
{
    this->setStyleSheet(theme);
    updateStyle(this);
}

void SendCustomFeeDialog::inform(const QString& text)
{
    if (!snackBar)
        snackBar = new SnackBar(nullptr, this);

    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

SendCustomFeeDialog::~SendCustomFeeDialog()
{
    delete ui;
}