// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SchillingCoin_ADDRESSBOOK_H
#define SchillingCoin_ADDRESSBOOK_H

#include <map>
#include <string>

namespace AddressBook {

    namespace AddressBookPurpose {
        extern const std::string UNKNOWN;
        extern const std::string RECEIVE;
        extern const std::string SEND;
        extern const std::string DELEGABLE;
        extern const std::string DELEGATOR;
        extern const std::string COLD_STAKING;
        extern const std::string COLD_STAKING_SEND;
    }

    bool IsColdStakingPurpose(const std::string& purpose);

/** Address book data */
    class CAddressBookData {
    public:

        std::string name;
        std::string purpose{AddressBook::AddressBookPurpose::UNKNOWN};

        using StringMap = std::map<std::string, std::string>;
        StringMap destdata;

        bool isSendColdStakingPurpose() const;
        bool isSendPurpose() const;
        bool isReceivePurpose() const;
    };

} // namespace AddressBook

#endif //SchillingCoin_ADDRESSBOOK_H