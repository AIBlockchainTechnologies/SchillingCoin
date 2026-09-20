// Copyright (c) 2011-2014, 2026 The Bitcoin developers
// Distributed under the MIT software license.

#ifndef BITCOIN_QT_PAYMENTSERVER_H
#define BITCOIN_QT_PAYMENTSERVER_H

#include "walletmodel.h"

#include <QObject>
#include <QString>
#include <QList>

class OptionsModel;
class CWallet;

QT_BEGIN_NAMESPACE
class QApplication;
class QByteArray;
class QLocalServer;
class QNetworkAccessManager;
class QNetworkReply;
class QSslError;
class QUrl;
QT_END_NAMESPACE

class PaymentServer : public QObject
{
    Q_OBJECT

public:
    static void ipcParseCommandLine(int argc, char* argv[]);
    static bool ipcSendCommandLine();

    // Temporary BIP-70 compatibility stub.
    static void LoadRootCAs();

    PaymentServer(QObject* parent, bool startLocalServer = true);
    ~PaymentServer();

    void setOptionsModel(OptionsModel* optionsModel);

Q_SIGNALS:
    void receivedPaymentRequest(SendCoinsRecipient);
    void message(const QString& title, const QString& message, unsigned int style);

public Q_SLOTS:
    void uiReady();
    void handleURIOrFile(const QString& s);

private Q_SLOTS:
    void handleURIConnection();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void initNetManager();

    bool saveURIs;
    QLocalServer* uriServer;
    QNetworkAccessManager* netManager;
    OptionsModel* optionsModel;
};

#endif // BITCOIN_QT_PAYMENTSERVER_H