// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechaindb.h"

#include "chain.h"
#include "core_io.h"
#include "primitives/transaction.h"
#include "sidechainclient.h"
#include "utilstrencodings.h"

#include <iostream>
#include <sstream>

SidechainDB::SidechainDB()
{
}
bool SidechainDeposit::operator==(const SidechainDeposit& a) const
{
    return (a.dtx == dtx &&
            a.keyID == keyID &&
            a.nSidechain == nSidechain);
}
std::vector<SidechainWT> SidechainDB::GetWTCache() const
{
    return vWTCache;
}

std::vector<SidechainDeposit> SidechainDB::GetDepositCache() const
{
    return vDepositCache;
}

bool SidechainDB::HaveDeposit(const SidechainDeposit& deposit) const
{
    for (const SidechainDeposit &d : vDepositCache) {
        if (d == deposit)
            return true;
    }
    return false;
}

std::string SidechainDB::ToString() const
{
    std::stringstream ss;
    return ss.str();
}

std::vector<SidechainDeposit> SidechainDB::UpdateDepositCache()
{
    // Request deposits from mainchain
    SidechainClient client;
    std::vector<SidechainDeposit> vDepositNew;
    vDepositNew = client.getDeposits(THIS_SIDECHAIN.nSidechain);

    // Find new unknown deposits
    std::vector<SidechainDeposit> vDepositNewUniq;
    for (const SidechainDeposit& d : vDepositNew) {
        if (HaveDeposit(d))
            continue;

        // Add new deposits to the cache and return vector
        vDepositCache.push_back(d);
        vDepositNewUniq.push_back(d);
    }

    return vDepositNewUniq;
}

std::string Sidechain::GetSidechainName() const
{
    return "SIDECHAIN_TEST";
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

std::string SidechainVerification::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nBlocksLeft=" << nBlocksLeft << std::endl;
    ss << "nWorkScore=" << nWorkScore << std::endl;
    return ss.str();
}

std::string SidechainDeposit::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "dtx=" << dtx.ToString() << std::endl;
    ss << "keyID=" << keyID.ToString() << std::endl;
    return ss.str();
}
