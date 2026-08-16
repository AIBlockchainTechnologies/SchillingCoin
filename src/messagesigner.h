// Copyright (c) 2014-2018 The Dash Core developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MESSAGESIGNER_H
#define MESSAGESIGNER_H

#include "key.h"
#include "primitives/transaction.h" // for CTxIn

enum MessageVersion {
    MESS_VER_STRMESS = 0,
    MESS_VER_HASH    = 1,
};

/** Helper class for signing messages and checking their signatures
 */
class CMessageSigner
{
public:
    static bool GetKeysFromSecret(const std::string& strSecret, CKey& keyRet, CPubKey& pubkeyRet);
    static uint256 GetMessageHash(const std::string& strMessage);
    static bool SignMessage(const std::string& strMessage, std::vector<unsigned char>& vchSigRet, const CKey& key);
    static bool VerifyMessage(const CPubKey& pubkey, const std::vector<unsigned char>& vchSig, const std::string& strMessage, std::string& strErrorRet);
    static bool VerifyMessage(const CKeyID& keyID, const std::vector<unsigned char>& vchSig, const std::string& strMessage, std::string& strErrorRet);
};

/** Helper class for signing hashes and checking their signatures
 */
class CHashSigner
{
public:
    static bool SignHash(const uint256& hash, const CKey& key, std::vector<unsigned char>& vchSigRet);
    static bool VerifyHash(const uint256& hash, const CPubKey& pubkey, const std::vector<unsigned char>& vchSig, std::string& strErrorRet);
    static bool VerifyHash(const uint256& hash, const CKeyID& keyID, const std::vector<unsigned char>& vchSig, std::string& strErrorRet);
};

/** Base Class for all signed messages on the network
 */
class CSignedMessage
{
protected:
    std::vector<unsigned char> vchSig;
    void swap(CSignedMessage& first, CSignedMessage& second);

public:
    int nMessVersion;

    CSignedMessage()
        : vchSig(),
          nMessVersion(MessageVersion::MESS_VER_HASH)
    {}

    // Copy constructor
    CSignedMessage(const CSignedMessage& other)
        : vchSig(other.vchSig),
          nMessVersion(other.nMessVersion)
    {}

    // Move constructor
    CSignedMessage(CSignedMessage&& other) noexcept
        : vchSig(std::move(other.vchSig)),
          nMessVersion(other.nMessVersion)
    {}

    // Copy assignment
    CSignedMessage& operator=(const CSignedMessage& other)
    {
        if (this != &other) {
            vchSig      = other.vchSig;
            nMessVersion = other.nMessVersion;
        }
        return *this;
    }

    // Move assignment
    CSignedMessage& operator=(CSignedMessage&& other) noexcept
    {
        if (this != &other) {
            vchSig       = std::move(other.vchSig);
            nMessVersion = other.nMessVersion;
        }
        return *this;
    }

    virtual ~CSignedMessage() {}

    // Sign-Verify message
    bool Sign(const CKey& key, const CPubKey& pubKey, const bool fNewSigs);
    bool Sign(const std::string strSignKey, const bool fNewSigs);
    bool CheckSignature(const CPubKey& pubKey) const;
    bool CheckSignature() const;

    // Pure virtual functions
    virtual uint256 GetSignatureHash() const = 0;
    virtual std::string GetStrMessage() const = 0;
    virtual const CTxIn GetVin() const = 0;

    virtual const CPubKey GetPublicKey(std::string& strErrorRet) const;

    void SetVchSig(const std::vector<unsigned char>& vchSigIn) { vchSig = vchSigIn; }
    std::vector<unsigned char> GetVchSig() const { return vchSig; }
    std::string GetSignatureBase64() const;
};

#endif