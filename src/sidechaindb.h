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

struct Verification {
    uint8_t nSidechain;
    uint16_t nBlocksLeft;
    uint16_t nWorkScore;
    uint256 wtxid;

    std::string ToString() const;
};

struct SidechainDeposit {
    uint8_t nSidechain;
    CKeyID keyID;
    CTransaction deposit;

    std::string ToString() const;
};

struct SidechainWT {
    uint8_t nSidechain;
    CKeyID keyID;
    CTransaction wt;

    std::string ToString() const;
};

struct SidechainWTJoin {
    uint8_t nSidechain;
    CKeyID keyID;
    CTransaction wtj;

    std::string ToString() const;
};

enum Sidechains {
    SIDECHAIN_TEST = 0,
//    SIDECHAIN_HIVEMIND = 1,
//    SIDECHAIN_WIMBLE = 2
};

static const Sidechain THIS_SIDECHAIN = {
    SIDECHAIN_TEST, 100, 200, 100, CScript() << OP_NOP4 /*OP_CHECKWORKSCORE*/
};

//! Max number of WT^(s) per sidechain per period
static const int SIDECHAIN_MAX_WT = 3;
//! State script version number
static const int SIDECHAIN_STATE_VERSION = 0;

class SidechainDB
{
    public:
        /** Resizes the DB to ARRAYLEN(ValidSidechains) */
        SidechainDB();

        /** Update the DB state if params are valid (used directly by unit tests) */
        bool Update(uint8_t nSidechain, uint16_t nBlocks, uint16_t nScore, uint256 wtxid);

        /** Create a script with OP_RETURN data representing the DB state */
        CScript CreateStateScript() const;

        /** Print the DB */
        std::string ToString() const;

    private:
        /** Internal representation of Sidechain(s) state */
        std::vector<std::vector<Verification>> DB;

        /** Make sure nSidechain first of all is not beyond the
          * size of DB[]. Also check that nSidechain is in the
          * list of valid sidechains */
        bool IndexValid(uint8_t nSidechain) const;

        /** Check that the internal DB is initialized. Return
          * true if at least one of the sidechains represented
          * by the DB has a current WT^ */
        bool HasState() const;
};

#endif // BITCOIN_SIDECHAINDB_H

