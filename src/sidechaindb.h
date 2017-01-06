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

    std::string ToString() const;
    std::string GetSidechainName() const;
};

struct SidechainDeposit {
    uint8_t nSidechain;
    CKeyID keyID;
    CTransaction dtx;

    std::string ToString() const;
    bool operator==(const SidechainDeposit& a) const;
};

struct SidechainVerification {
    uint8_t nSidechain;
    uint16_t nBlocksLeft;
    uint16_t nWorkScore;
    uint256 wtxid;

    std::string ToString() const;
};

enum Sidechains {
    SIDECHAIN_TEST = 0,
    SIDECHAIN_HIVEMIND = 1,
    SIDECHAIN_WIMBLE = 2
};

static const Sidechain ValidSidechains[] =
{
    // {nSidechain, nWaitPeriod, nVerificationPeriod, nMinWorkScore}
    {SIDECHAIN_TEST, 100, 200, 100},
    {SIDECHAIN_HIVEMIND, 200, 400, 200},
    {SIDECHAIN_WIMBLE, 200, 400, 200},
};

//! Max number of WT^(s) per sidechain per period
static const int SIDECHAIN_MAX_WT = 3;

/** Make sure nSidechain first of all is not beyond the
  * size of DB[]. Also check that nSidechain is in the
  * list of valid sidechains */
bool SidechainNumberValid(uint8_t nSidechain);

class SidechainDB
{
    public:
        /** Resizes the DB to ARRAYLEN(ValidSidechains) */
        SidechainDB();

        /** Add a new WT^ to the database */
        bool AddSidechainWTJoin(uint8_t nSidechain, CTransaction wtx);

        /** Add deposit to cache */
        bool AddSidechainDeposit(const CTransaction &tx);

        /** Update DB state with new state script */
        bool Update(const CScript& script);

        /** Update the DB state if params are valid (used directly by unit tests) */
        bool Update(uint8_t nSidechain, uint16_t nBlocks, uint16_t nScore, uint256 wtxid);

        /** Check that sufficient workscore exists for WT^ */
        bool CheckWorkScore(uint8_t nSidechain, uint256 wtxid) const;

        /** Return the most verified WT^ (full transaction) for nSidechain */
        CTransaction GetWTJoin(uint8_t nSidechain) const;

        /** Return true if the full WT^ CTransaction is cached */
        bool HaveWTJoinCached(uint256 wtxid) const;

        /** Return true if the deposit is cached */
        bool HaveDepositCached(SidechainDeposit deposit) const;

        /** Get all of the deposits this period for nSidechain. */
        std::vector<SidechainDeposit> GetSidechainDeposits(uint8_t nSidechain) const;

        /** Create a script with OP_RETURN data representing the DB state */
        CScript CreateStateScript() const;

        /** Print the DB */
        std::string ToString() const;

    private:
        /** Internal representation of Sidechain(s) state */
        std::vector<std::vector<SidechainVerification>> DB;

        /** The DB vector stores verifications, which contain as one member
         *  the hash / txid of the WT^ being verified. This vector stores the
         *  full transaction(s) so that they can be looked up as needed */
        std::vector<CTransaction> vWTJoinCache;

        /** Track deposits created during this period */
        std::vector<SidechainDeposit> vDepositCache;

        /** Get all of the current WT^(s) for nSidechain.
          * Note that this is different than GetWT(nSidechain) which
          * returns only a single WT^ to pay out if one exists for the
          * current period */
        std::vector<std::vector<SidechainVerification>> GetSidechainWTs(uint8_t nSidechain) const;

        /** Check that the internal DB is initialized. Return
          * true if at least one of the sidechains represented
          * by the DB has a current WT^ */
        bool HasState() const;
};

#endif // BITCOIN_SIDECHAINDB_H

