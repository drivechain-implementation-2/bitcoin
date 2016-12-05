// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechaindb.h"

#include "chain.h"
#include "core_io.h"
#include "primitives/transaction.h"
#include "utilstrencodings.h"

#include <iostream>
#include <sstream>

SidechainDB::SidechainDB()
{
}

bool SidechainDB::Update(uint8_t nSidechain, uint16_t nBlocks, uint16_t nScore, uint256 wtxid)
{
    if (!IndexValid(nSidechain))
        return false;

    Verification v;
    v.nBlocksLeft = nBlocks;
    v.nSidechain = nSidechain;
    v.nWorkScore = nScore;
    v.wtxid = wtxid;

    DB[nSidechain].push_back(v);

    return true;
}

std::string SidechainDB::ToString() const
{
    std::stringstream ss;
    return ss.str();
}

bool SidechainDB::HasState() const
{
    if (!DB.size())
        return false;

    if (!DB[SIDECHAIN_TEST].empty())
        return true;

    return false;
}

std::string Sidechain::GetSidechainName() const
{
    // Check that number coresponds to a valid sidechain
    switch (nSidechain) {
    case SIDECHAIN_TEST:
        return "SIDECHAIN_TEST";
    default:
        break;
    }
    return "SIDECHAIN_UNKNOWN";
}

std::string Sidechain::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nWaitPeriod=" << nWaitPeriod << std::endl;
    ss << "nVerificationPeriod=" << nVerificationPeriod << std::endl;
    ss << "nMinWorkScore=" << nMinWorkScore << std::endl;
    ss << "depositScript=" << ScriptToAsmStr(depositScript) << std::endl;
    return ss.str();
}

std::string Verification::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nBlocksLeft=" << nBlocksLeft << std::endl;
    ss << "nWorkScore=" << nWorkScore << std::endl;
    return ss.str();
}
