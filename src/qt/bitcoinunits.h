// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINUNITS_H
#define BITCOIN_QT_BITCOINUNITS_H

#include "amount.h"

#include <QAbstractListModel>
#include <QString>
#include <vector>

// U+2009 THIN SPACE = UTF-8 E2 80 89
#define REAL_THIN_SP_CP 0x2009
#define REAL_THIN_SP_UTF8 "\xE2\x80\x89"
#define REAL_THIN_SP_HTML "&thinsp;"

#define COMMA_CP 0x2C
#define COMMA_UTF8 "\x2C"
#define COMMA_HTML "&#44;"

// U+200A HAIR SPACE = UTF-8 E2 80 8A
#define HAIR_SP_CP 0x200A
#define HAIR_SP_UTF8 "\xE2\x80\x8A"
#define HAIR_SP_HTML "&#8202;"

// U+2006 SIX-PER-EM SPACE = UTF-8 E2 80 86
#define SIXPEREM_SP_CP 0x2006
#define SIXPEREM_SP_UTF8 "\xE2\x80\x86"
#define SIXPEREM_SP_HTML "&#8198;"

// U+2007 FIGURE SPACE = UTF-8 E2 80 87
#define FIGURE_SP_CP 0x2007
#define FIGURE_SP_UTF8 "\xE2\x80\x87"
#define FIGURE_SP_HTML "&#8199;"

#define HTML_HACK_SP "<span style='white-space: nowrap; font-size: 6pt'> </span>"

#define THIN_SP_CP REAL_THIN_SP_CP
#define THIN_SP_UTF8 REAL_THIN_SP_UTF8
#define THIN_SP_HTML HTML_HACK_SP

/** SchillingCoin unit definitions. Encapsulates parsing and formatting
   and serves as list model for drop-down selection boxes.
*/
class BitcoinUnits : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit BitcoinUnits(QObject* parent);

    enum Unit {
        SCH,
        mSCH,
        uSCH
    };

    enum SeparatorStyle {
        separatorNever,
        separatorStandard,
        separatorAlways
    };

    /// Static API

    //! Get list of units, for drop-down box
    static std::vector<Unit> availableUnits();

    static bool valid(int unit);
    static QString id(int unit);
    static QString name(int unit);
    static QString description(int unit);
    static qint64 factor(int unit);
    static int decimals(int unit);
    static QString format(int unit, const CAmount& amount, bool plussign = false,
                          SeparatorStyle separators = separatorStandard,
                          bool cleanRemainderZeros = true);
    static QString simpleFormat(int unit, const CAmount& amount, bool plussign = false,
                                SeparatorStyle separators = separatorStandard);
    static QString formatWithUnit(int unit, const CAmount& amount, bool plussign = false,
                                  SeparatorStyle separators = separatorStandard);
    static QString formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign = false,
                                      SeparatorStyle separators = separatorStandard);
    static QString floorWithUnit(int unit, const CAmount& amount, bool plussign = false,
                                 SeparatorStyle separators = separatorStandard,
                                 bool cleanRemainderZeros = false);
    static QString floorHtmlWithUnit(int unit, const CAmount& amount, bool plussign = false,
                                     SeparatorStyle separators = separatorStandard,
                                     bool cleanRemainderZeros = false);
    static bool parse(int unit, const QString& value, CAmount* val_out);
    static QString getAmountColumnTitle(int unit);

    enum RoleIndex {
        UnitRole = Qt::UserRole
    };

    int rowCount(const QModelIndex& parent) const;
    QVariant data(const QModelIndex& index, int role) const;

    static QString removeSpaces(QString text)
    {
        text.remove(' ');
        text.remove(QChar(THIN_SP_CP));
#if (THIN_SP_CP != REAL_THIN_SP_CP)
        text.remove(QChar(REAL_THIN_SP_CP));
#endif
        return text;
    }

    static CAmount maxMoney();

private:
    std::vector<Unit> unitlist;
};

typedef BitcoinUnits::Unit BitcoinUnit;

#endif // BITCOIN_QT_BITCOINUNITS_H