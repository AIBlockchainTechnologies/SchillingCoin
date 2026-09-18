// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/schillingcoin/masternodewizarddialog.h"
#include "qt/schillingcoin/forms/ui_masternodewizarddialog.h"

#include "activemasternode.h"
#include "optionsmodel.h"
#include "pairresult.h"
#include "qt/schillingcoin/mnmodel.h"
#include "qt/schillingcoin/guitransactionsutils.h"
#include "qt/schillingcoin/qtutils.h"

#include <QFile>
#include <QIntValidator>
#include <QHostAddress>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

MasterNodeWizardDialog::MasterNodeWizardDialog(WalletModel *model, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MasterNodeWizardDialog),
    icConfirm1(new QPushButton()),
    icConfirm3(new QPushButton()),
    icConfirm4(new QPushButton()),
    walletModel(model)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());
    setCssProperty(ui->frame, "container-dialog");
    ui->frame->setContentsMargins(10,10,10,10);

    setCssProperty({ui->labelLine1, ui->labelLine3}, "line-purple");
    setCssProperty({ui->groupBoxName, ui->groupContainer}, "container-border");
    setCssProperty({ui->pushNumber1, ui->pushNumber3, ui->pushNumber4}, "btn-number-check");
    setCssProperty({ui->pushName1, ui->pushName3, ui->pushName4}, "btn-name-check");

    ui->pushNumber1->setEnabled(false);
    ui->pushNumber3->setEnabled(false);
    ui->pushNumber4->setEnabled(false);
    ui->pushName1->setEnabled(false);
    ui->pushName3->setEnabled(false);
    ui->pushName4->setEnabled(false);

    // Frame 1
    setCssProperty(ui->labelTitle1, "text-title-dialog");
    setCssProperty(ui->labelMessage1a, "text-main-grey");
    setCssProperty(ui->labelMessage1b, "text-main-purple");

    // Frame 3
    setCssProperty(ui->labelTitle3, "text-title-dialog");
    setCssProperty(ui->labelMessage3, "text-main-grey");

    ui->lineEditName->setPlaceholderText(tr("e.g user_masternode"));
    initCssEditLine(ui->lineEditName);
    QRegularExpression rx("^(?:(?![\\#\\s]).)*");
    ui->lineEditName->setValidator(new QRegularExpressionValidator(rx, ui->lineEditName));

    // Frame 4
    setCssProperty(ui->labelTitle4, "text-title-dialog");
    setCssProperty({ui->labelSubtitleIp, ui->labelSubtitlePort}, "text-title");
    setCssSubtitleScreen(ui->labelSubtitleAddressIp);

    ui->lineEditIpAddress->setPlaceholderText("e.g 18.255.255.255");
    ui->lineEditPort->setPlaceholderText("e.g 9070");
    initCssEditLine(ui->lineEditIpAddress);
    initCssEditLine(ui->lineEditPort);
    ui->stackedWidget->setCurrentIndex(pos);
    ui->lineEditPort->setEnabled(false);
    if (walletModel->isRegTestNetwork()) {
        ui->lineEditPort->setText("51476");
    } else if (walletModel->isTestNetwork()) {
        ui->lineEditPort->setText("51474");
    } else {
        ui->lineEditPort->setText("9070");
    }

    ui->stackedIcon1->addWidget(icConfirm1);
    ui->stackedIcon3->addWidget(icConfirm3);
    ui->stackedIcon4->addWidget(icConfirm4);
    initBtn({icConfirm1, icConfirm3, icConfirm4});
    setCssProperty({icConfirm1, icConfirm3, icConfirm4}, "ic-step-confirm");

    setCssBtnPrimary(ui->btnNext);
    ui->btnNext->setText(tr("NEXT"));
    setCssProperty(ui->btnBack , "btn-dialog-cancel");
    ui->btnBack->setVisible(false);
    ui->btnBack->setText(tr("BACK"));
    setCssProperty(ui->pushButtonSkip, "ic-close");

    connect(ui->pushButtonSkip, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->btnNext, SIGNAL(clicked()), this, SLOT(onNextClicked()));
    connect(ui->btnBack, SIGNAL(clicked()), this, SLOT(onBackClicked()));
}

void MasterNodeWizardDialog::showEvent(QShowEvent *event)
{
    if (ui->btnNext) ui->btnNext->setFocus();
}

void MasterNodeWizardDialog::onNextClicked()
{
    switch(pos) {
        case 0:{
            ui->stackedWidget->setCurrentIndex(1);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(true);
            ui->pushNumber3->setChecked(true);
            ui->btnBack->setVisible(true);
            ui->lineEditName->setFocus();
            break;
        }
        case 1:{
            if (ui->lineEditName->text().isEmpty()) {
                setCssEditLine(ui->lineEditName, false, true);
                return;
            }
            setCssEditLine(ui->lineEditName, true, true);

            ui->stackedWidget->setCurrentIndex(2);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            ui->pushName1->setChecked(true);
            icConfirm3->setVisible(true);
            ui->pushNumber4->setChecked(true);
            ui->btnBack->setVisible(true);
            ui->lineEditIpAddress->setFocus();
            break;
        }
        case 2:{
            if (ui->lineEditIpAddress->text().isEmpty()) {
                return;
            }

            icConfirm4->setVisible(true);
            ui->btnBack->setVisible(true);
            isOk = createMN();
            accept();
        }
    }
    pos++;
}

bool MasterNodeWizardDialog::createMN()
{
    if (!walletModel) {
        returnStr = tr("walletModel not set");
        return false;
    }

    QString addressLabel = ui->lineEditName->text();
    if (addressLabel.isEmpty()) {
        returnStr = tr("address label cannot be empty");
        return false;
    }
    std::string alias = addressLabel.toStdString();

    QString addressStr = ui->lineEditIpAddress->text();
    QString portStr = ui->lineEditPort->text();
    if (addressStr.isEmpty() || portStr.isEmpty()) {
        returnStr = tr("IP or port cannot be empty");
        return false;
    }
    if (!MNModel::validateMNIP(addressStr)) {
        returnStr = tr("Invalid IP address");
        return false;
    }

    std::string ipAddress = addressStr.toStdString();
    std::string port = portStr.toStdString();

    CKey secret;
    secret.MakeNewKey(false);
    CBitcoinSecret mnKey = CBitcoinSecret(secret);
    std::string mnKeyString = mnKey.ToString();

    COutPoint collateralOut;

    if (!walletModel->getMNCollateralCandidate(collateralOut)) {
        CBitcoinAddress address;
        PairResult r = walletModel->getNewAddress(address, alias);

        if (!r.result) {
            inform(tr(r.status->c_str()));
            return false;
        }

        SendCoinsRecipient sendCoinsRecipient(QString::fromStdString(address.ToString()),
                                              QString::fromStdString(alias),
                                              GetMNCollateral() * COIN,
                                              "");

        // QList → std::vector modernization
        std::vector<SendCoinsRecipient> recipients;
        recipients.push_back(sendCoinsRecipient);

        WalletModelTransaction currentTransaction(recipients);
        WalletModel::SendCoinsReturn prepareStatus;

        prepareStatus = walletModel->prepareTransaction(currentTransaction);

        QString returnMsg = "Unknown error";
        CClientUIInterface::MessageBoxFlags informType;
        returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(
                this,
                prepareStatus,
                walletModel,
                informType,
                BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                             currentTransaction.getTransactionFee()),
                true
        );

        if (prepareStatus.status != WalletModel::OK) {
            returnStr = tr("Prepare master node failed.\n\n%1\n").arg(returnMsg);
            return false;
        }

        WalletModel::SendCoinsReturn sendStatus = walletModel->sendCoins(currentTransaction);
        returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(
                this,
                sendStatus,
                walletModel,
                informType
        );

        if (sendStatus.status != WalletModel::OK) {
            returnStr = tr("Cannot send collateral transaction.\n\n%1").arg(returnMsg);
            return false;
        }

        CWalletTx* walletTx = currentTransaction.getTransaction();
        std::string txID = walletTx->GetHash().GetHex();
        int indexOut = -1;
        for (int i = 0; i < (int)walletTx->vout.size(); i++) {
            CTxOut& out = walletTx->vout[i];
            if (out.nValue == GetMNCollateral() * COIN) {
                indexOut = i;
                break;
            }
        }
        if (indexOut == -1) {
            returnStr = tr("Invalid collateral output index");
            return false;
        }

        collateralOut = COutPoint(walletTx->GetHash(), indexOut);
    }

    std::string strConfFile = "masternode.conf";
    std::string strDataDir = GetDataDir().string();
    if (strConfFile != boost::filesystem::basename(strConfFile) + boost::filesystem::extension(strConfFile)) {
        throw std::runtime_error(strprintf(_("masternode.conf %s resides outside data directory %s"), strConfFile, strDataDir));
    }

    boost::filesystem::path pathBootstrap = GetDataDir() / strConfFile;
    if (!boost::filesystem::exists(pathBootstrap)) {
        returnStr = tr("masternode.conf file doesn't exists");
        return false;
    }

    boost::filesystem::path pathMasternodeConfigFile = GetMasternodeConfigFile();
    boost::filesystem::ifstream streamConfig(pathMasternodeConfigFile);

    if (!streamConfig.good()) {
        returnStr = tr("Invalid masternode.conf file");
        return false;
    }

    int linenumber = 1;
    std::string lineCopy = "";
    for (std::string line; std::getline(streamConfig, line); linenumber++) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string comment, alias, ip, privKey, txHash, outputIndex;

        if (iss >> comment) {
            if (comment.at(0) == '#') continue;
            iss.str(line);
            iss.clear();
        }

        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                streamConfig.close();
                returnStr = tr("Error parsing masternode.conf file");
                return false;
            }
        }
        lineCopy += line + "\n";
    }

    if (lineCopy.size() == 0) {
        lineCopy =
            "# Masternode config file\n"
            "# Format: alias IP:port masternodeprivkey collateral_output_txid collateral_output_index\n"
            "# Example: mn1 127.0.0.2:51472 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0"
            "#";
    }
    lineCopy += "\n";

    streamConfig.close();

    std::string txID = collateralOut.hash.ToString();
    std::string indexOutStr = std::to_string(collateralOut.n);

    QHostAddress hostAddress(addressStr);
    QAbstractSocket::NetworkLayerProtocol layerProtocol = hostAddress.protocol();
    if (layerProtocol == QAbstractSocket::IPv6Protocol) {
        ipAddress = "[" + ipAddress + "]";
    }

    boost::filesystem::path pathConfigFile("masternode_temp.conf");
    if (!pathConfigFile.is_complete()) pathConfigFile = GetDataDir() / pathConfigFile;
    FILE* configFile = fopen(pathConfigFile.string().c_str(), "w");
    lineCopy += alias + " " + ipAddress + ":" + port + " " + mnKeyString + " " + txID + " " + indexOutStr + "\n";
    fwrite(lineCopy.c_str(), std::strlen(lineCopy.c_str()), 1, configFile);
    fclose(configFile);

    boost::filesystem::path pathOldConfFile("old_masternode.conf");
    if (!pathOldConfFile.is_complete()) pathOldConfFile = GetDataDir() / pathOldConfFile;
    if (boost::filesystem::exists(pathOldConfFile)) {
        boost::filesystem::remove(pathOldConfFile);
    }
    rename(pathMasternodeConfigFile, pathOldConfFile);

    boost::filesystem::path pathNewConfFile("masternode.conf");
    if (!pathNewConfFile.is_complete()) pathNewConfFile = GetDataDir() / pathNewConfFile;
    rename(pathConfigFile, pathNewConfFile);

    mnEntry = masternodeConfig.add(alias, ipAddress + ":" + port, mnKeyString, txID, indexOutStr);

    walletModel->lockCoin(collateralOut);

    returnStr = tr("Masternode created! Wait %1 confirmations before starting it.").arg(MASTERNODE_MIN_CONFIRMATIONS);
    return true;
}

void MasterNodeWizardDialog::onBackClicked()
{
    if (pos == 0) return;
    pos--;
    switch(pos) {
        case 0:{
            ui->stackedWidget->setCurrentIndex(0);
            ui->btnNext->setFocus();
            ui->pushNumber1->setChecked(true);
            ui->pushNumber4->setChecked(false);
            ui->pushNumber3->setChecked(false);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(false);
            ui->pushName1->setChecked(true);
            icConfirm1->setVisible(false);
            ui->btnBack->setVisible(false);
            break;
        }
        case 1:{
            ui->stackedWidget->setCurrentIndex(1);
            ui->lineEditName->setFocus();
            ui->pushNumber4->setChecked(false);
            ui->pushNumber3->setChecked(true);
            ui->pushName4->setChecked(false);
            ui->pushName3->setChecked(true);
            icConfirm3->setVisible(false);
            break;
        }
    }
}

void MasterNodeWizardDialog::inform(QString text)
{
    if (!snackBar)
        snackBar = new SnackBar(nullptr, this);
    snackBar->setText(text);
    snackBar->resize(this->width(), snackBar->height());
    openDialog(snackBar, this);
}

QSize BUTTON_SIZE = QSize(22, 22);
void MasterNodeWizardDialog::initBtn(std::initializer_list<QPushButton*> args)
{
    for (QPushButton* btn : args) {
        btn->setMinimumSize(BUTTON_SIZE);
        btn->setMaximumSize(BUTTON_SIZE);
        btn->move(0, 0);
        btn->show();
        btn->raise();
        btn->setVisible(false);
    }
}

MasterNodeWizardDialog::~MasterNodeWizardDialog()
{
    if (snackBar) delete snackBar;
    delete ui;
}