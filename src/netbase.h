// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NETBASE_H
#define BITCOIN_NETBASE_H

#if defined(HAVE_CONFIG_H)
#include "config/schillingcoin-config.h"
#endif

#include "compat.h"
#include "serialize.h"
#include "netaddress.h"

#include <stdint.h>
#include <string>
#include <vector>

extern int nConnectTimeout;
extern bool fNameLookup;

static const int DEFAULT_CONNECT_TIMEOUT = 5000;
static const int DEFAULT_NAME_LOOKUP = true;

class proxyType
{
public:
    proxyType(): randomize_credentials(false) {}
    proxyType(const CService &proxy, bool randomize_credentials=false)
        : proxy(proxy), randomize_credentials(randomize_credentials) {}

    bool IsValid() const { return proxy.IsValid(); }

    CService proxy;
    bool randomize_credentials;
};

enum Network ParseNetwork(std::string net);
std::string GetNetworkName(enum Network net);
void SplitHostPort(std::string in, int& portOut, std::string& hostOut);

bool SetProxy(enum Network net, const proxyType &addrProxy);
bool GetProxy(enum Network net, proxyType& proxyInfoOut);
bool IsProxy(const CNetAddr& addr);

bool SetNameProxy(const proxyType &addrProxy);
bool HaveNameProxy();

bool LookupHost(const char* pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions, bool fAllowLookup);
bool Lookup(const char* pszName, CService& addr, int portDefault, bool fAllowLookup);
bool Lookup(const char* pszName, std::vector<CService>& vAddr, int portDefault, bool fAllowLookup, unsigned int nMaxSolutions);
bool LookupNumeric(const char* pszName, CService& addr, int portDefault = 0);

bool ConnectSocket(const CService& addr, SOCKET& hSocketRet, int nTimeout, bool* outProxyConnectionFailed = 0);
bool ConnectSocketByName(CService& addr, SOCKET& hSocketRet, const char* pszDest, int portDefault, int nTimeout, bool* outProxyConnectionFailed = 0);

std::string NetworkErrorString(int err);
bool CloseSocket(SOCKET& hSocket);
bool SetSocketNonBlocking(SOCKET& hSocket, bool fNonBlocking);

struct timeval MillisToTimeval(int64_t nTimeout);

#endif // BITCOIN_NETBASE_H
