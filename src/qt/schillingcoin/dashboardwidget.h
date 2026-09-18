// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include "qt/schillingcoin/pwidget.h"
#include "qt/schillingcoin/furabstractlistitemdelegate.h"
#include "qt/schillingcoin/furlistrow.h"
#include "transactiontablemodel.h"
#include "qt/schillingcoin/txviewholder.h"
#include "transactionfilterproxy.h"

#include <atomic>
#include <cstdlib>
#include <vector>

#include <QWidget>
#include <QLineEdit>
#include <QMap>
#include <QStringList>

#if defined(HAVE_CONFIG_H)
#include "config/schillingcoin-config.h" /* for USE_QTCHARTS */
#endif

#ifdef USE_QTCHARTS

#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE
using namespace QtCharts;

#endif

class SchillingCoinGUI;
class WalletModel;

namespace Ui {
class DashboardWidget;
}

class SortEdit : public QLineEdit{
    Q_OBJECT
public:
    explicit SortEdit(QWidget* parent = nullptr) : QLineEdit(parent){}

    inline void mousePressEvent(QMouseEvent *) override{
        Q_EMIT Mouse_Pressed();
    }

    ~SortEdit() override{}

Q_SIGNALS:
    void Mouse_Pressed();
};

enum SortTx {
    DATE_ASC = 0,
    DATE_DESC = 1,
    AMOUNT_ASC = 2,
    AMOUNT_DESC = 3
};

enum ChartShowType {
    ALL,
    YEAR,
    MONTH,
    DAY
};

class ChartData {
public:
    ChartData() {}

    QMap<int, std::pair<qint64, qint64>> amountsByCache;
    qreal maxValue = 0;
    qint64 totalStakes = 0;
    qint64 totalMNRewards = 0;

    // QList → std::vector modernization
    std::vector<qreal> valuesStakes;
    std::vector<qreal> valuesMNRewards;

    QStringList xLabels;
};

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class DashboardWidget : public PWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(SchillingCoinGUI* _window);
    ~DashboardWidget();

    void loadWalletModel() override;
    void loadChart();

    void run(int type) override;
    void onError(QString error, int type) override;

public Q_SLOTS:
    void walletSynced(bool isSync);
    void processNewTransaction(const QModelIndex& parent, int start, int /*end*/);

Q_SIGNALS:
    void incomingTransaction(const QString& date, int unit, const CAmount& amount,
                             const QString& type, const QString& address);

private Q_SLOTS:
    void handleTransactionClicked(const QModelIndex &index);
    void changeTheme(bool isLightTheme, QString &theme) override;
    void onSortChanged(const QString&);
    void onSortTypeChanged(const QString& value);
    void updateDisplayUnit();
    void showList();
    void onTxArrived(const QString& hash, const bool& isCoinStake,
                     const bool& isCSAnyType, const bool& isMasternodeReward);

#ifdef USE_QTCHARTS
    void windowResizeEvent(QResizeEvent *event);
    void changeChartColors();
    void onChartYearChanged(const QString&);
    void onChartMonthChanged(const QString&);
    void onChartArrowClicked(bool goLeft);
#endif

private:
    Ui::DashboardWidget *ui;
    FurAbstractListItemDelegate* txViewDelegate;
    TransactionFilterProxy* filter;
    TxViewHolder* txHolder;
    TransactionTableModel* txModel;
    int nDisplayUnit = -1;
    bool isSync = false;

#ifdef USE_QTCHARTS

    int64_t lastRefreshTime = 0;
    std::atomic<bool> isLoading;

    TransactionFilterProxy* stakesFilter = nullptr;
    bool isChartInitialized = false;
    QChartView *chartView = nullptr;
    QBarSeries *series = nullptr;
    QBarSet *set0 = nullptr;

    QBarCategoryAxis *axisX = nullptr;
    QValueAxis *axisY = nullptr;

    QChart *chart = nullptr;
    bool isChartMin = false;
    ChartShowType chartShow = YEAR;
    int yearFilter = 0;
    int monthFilter = 0;
    int dayStart = 1;

    ChartData* chartData = nullptr;
    bool hasStakes = false;

    void initChart();
    void showHideEmptyChart(bool show, bool loading, bool forceView = false);
    bool refreshChart();
    void tryChartRefresh();
    void updateStakeFilter();
    const QMap<int, std::pair<qint64, qint64>> getAmountBy();
    bool loadChartData(bool withMonthNames);
    void updateAxisX(const QStringList *arg = nullptr);
    void setChartShow(ChartShowType type);
    std::pair<int, int> getChartRange(QMap<int, std::pair<qint64, qint64>> amountsBy);

private Q_SLOTS:
    void onChartRefreshed();

#endif

};

#endif // DASHBOARDWIDGET_H