// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECHAINDB_H
#define BITCOIN_SIDECHAINDB_H

#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/script.h"
#include "uint256.h"

#include <cstdint>
#include <string>
#include <vector>

struct Sidechain {
    uint8_t nSidechain;
    uint16_t nWaitPeriod;
    uint16_t nVerificationPeriod;
    uint16_t nMinWorkScore;
    CScript depositScript;

    std::string ToString() const;
    std::string GetSidechainName() const;
};

struct SidechainVerification {
    uint8_t nSidechain;
    uint16_t nBlocksLeft;
    uint16_t nWorkScore;
    uint256 wtxid;

    std::string ToString() const;
};

struct SidechainDeposit {
    uint8_t nSidechain;
    CKeyID keyID;
    CTransaction dtx;

    std::string ToString() const;
    bool operator==(const SidechainDeposit& a) const;
};

struct SidechainWT {
    uint8_t nSidechain;
    CKeyID keyID;
    CTransaction wt;

    std::string ToString() const;
    bool operator==(const SidechainWT& a) const;
};

//! KeyID for testing
static const std::string testkey = "b5437dc6a4e5da5597548cf87db009237d286636";
//mx3PT9t2kzCFgAURR9HeK6B5wN8egReUxY
//cN5CqwXiaNWhNhx3oBQtA8iLjThSKxyZjfmieTsyMpG6NnHBzR7J

enum Sidechains {
    SIDECHAIN_TEST = 0,
};

//! This sidechain as it has been described to the mainchain
static const Sidechain THIS_SIDECHAIN = {
    SIDECHAIN_TEST, 100, 200, 100, CScript() << THIS_SIDECHAIN.nSidechain << ToByteVector(testkey) << OP_NOP4
};

//! This sidechain's fee script
//static const CScript SIDECHAIN_FEE_SCRIPT = CScript() << OP_TRUE;

static const CScript SIDECHAIN_FEESCRIPT = CScript() << OP_DUP << OP_HASH160 << ToByteVector(testkey) << OP_EQUALVERIFY << OP_CHECKSIG;

//! Max number of WT^(s) per sidechain per period
static const int SIDECHAIN_MAX_WT = 3;
//! State script version number
static const int SIDECHAIN_STATE_VERSION = 0;

class SidechainDB
{
    public:
        SidechainDB();

        /** Return const copy of current WT(s) */
        std::vector<SidechainWT> GetWTCache() const;

        /** Return const copy of current deposits */
        std::vector<SidechainDeposit> GetDepositCache() const;

        /** Request deposits from the mainchain, add any unknown
         *  deposits to the cache and return them in a vector */
        std::vector<SidechainDeposit> UpdateDepositCache();

        std::vector<CTransaction> GetWTJoinCache();

        /** Update WT^ cache */
        bool AddSidechainWTJoin(const CTransaction& tx);

        /** Update wt cache */
        bool AddSidechainWT(const CTransaction& tx);

        /** Return true if cache already has deposit */
        bool HaveDeposit(const SidechainDeposit& deposit) const;

        /** Return true if cache already has wt */
        bool HaveWT(const SidechainWT& wt) const;

        /** Return true if full CTransaction with WT^ txid is cached */
        bool HaveWTJoin(const uint256& txid) const;

        /** Print the DB */
        std::string ToString() const;

    private:
        /** Sidechain wt(s) created this period */
        std::vector<SidechainWT> vWTCache;

        /** Sidechain WT^ cache */
        std::vector<CTransaction> vWTJoinCache;

        /** Sidechain deposit(s) this period.
         *  Updated when deposit is paid out by miner and
         *  when block is connected, if not a duplicate */
        std::vector<SidechainDeposit> vDepositCache;
};

#endif // BITCOIN_SIDECHAINDB_H

