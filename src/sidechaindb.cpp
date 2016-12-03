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
    DB.resize(ARRAYLEN(ValidSidechains));
}

bool SidechainDB::AddSidechainWT(uint8_t nSidechain, CTransaction wtx)
{
    if (!IndexValid(nSidechain))
        return false;

    std::vector<std::vector<Verification>> vWT;
    vWT = GetSidechainWTs(nSidechain);

    if (!(vWT.size() < SIDECHAIN_MAX_WT))
        return false;

    const Sidechain& s = ValidSidechains[nSidechain];
    uint16_t tau = s.nWaitPeriod + s.nVerificationPeriod;

    return Update(nSidechain, tau, 0, wtx.GetHash());
}

bool SidechainDB::Update(const CScript& state)
{
    // TODO refactor this messy function

    // If there aren't any known WT^(s) there is nothing to update
    if (!HasState())
        return false;

    // State script cannot be smaller than this
    if (state.size() < 5)
        return false;

    CScript::const_iterator pc = state.begin();
    std::vector<unsigned char> vch;
    opcodetype opcode;

    // Get the opcode
    if (!state.GetOp2(pc, opcode, &vch))
        return false;
    if (opcode != OP_RETURN)
        return false;

    // Get the data
    std::vector<unsigned char> data;
    data.assign(state.begin() + 2, state.begin() + state.size());

    if (data.size() < 3)
        return false;
    if (data[0] != SIDECHAIN_STATE_VERSION || data[1] != ':')
        return false;

    uint8_t nSidechain = 0;
    size_t nWTIndex = 0;
    for (size_t i = 2; i < data.size(); i++) {
        if (!IndexValid(nSidechain))
            return false;
        if (data[i] == '.') {
            nWTIndex++;
            continue;
        }
        if (data[i] == '|') {
            nWTIndex = 0;
            nSidechain++;
            continue;
        }

        if (data[i] != 0 && data[i] != 1 && data[i] != 2)
            continue;

        std::vector<std::vector<Verification>> vWT = GetSidechainWTs(nSidechain);
        if (nWTIndex > vWT.size())
            return false;

        const Verification& old = vWT[nWTIndex].back();

        if (data[i] == 0)
            return Update(old.nSidechain, old.nBlocksLeft - 1, old.nWorkScore - 1, old.wtxid);
        else
        if (data[i] == 1)
            return Update(old.nSidechain, old.nBlocksLeft - 1, old.nWorkScore + 1, old.wtxid);
        else
        if (data[i] == 2)
            return Update(old.nSidechain, old.nBlocksLeft - 1, old.nWorkScore, old.wtxid);
    }
    return false;
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

bool SidechainDB::CheckWorkScore(uint8_t nSidechain, uint256 wtxid) const
{
    if (!IndexValid(nSidechain))
        return false;

    std::vector<std::vector<Verification>> wtVerifications;
    wtVerifications = GetSidechainWTs(nSidechain);

    for (size_t x = 0; x < wtVerifications.size(); x++) {
        if (!wtVerifications[x].size())
            continue;
        if (wtVerifications[x][0].wtxid != wtxid)
            continue;

        uint32_t nScore = 0;
        for (size_t y = 0; y < wtVerifications[x].size(); y++) {
            if (std::abs(wtVerifications[x][y].nWorkScore - nScore) > 1)
                continue;
            nScore = wtVerifications[x][y].nWorkScore;
        }

        if (nScore >= ValidSidechains[nSidechain].nMinWorkScore)
            return true;
    }

    return false;
}

CTransaction SidechainDB::GetWT(uint8_t nSidechain) const
{
    // Return the raw transaction of WT^ which was
    // sent to this bitcoind by the sidechain daemon
    // on localhost.
    CTransaction wtx;
    return wtx;
}

CScript SidechainDB::CreateStateScript() const
{
    /* TODO
     * use merged mining to decide what the new state is.
     * For now, just upvoting the current best WT^.
     */
    if (!HasState())
        return CScript();

    // Create a stream, add the current state script version
    std::ostringstream os(std::ios::binary);
    os << std::hex << SIDECHAIN_STATE_VERSION << ':';

    // Add the current DB state to the stream Ex: hex(0:0.1.2|0|0.1)
    for (size_t x = 0; x < DB.size(); x++) {
        std::vector<std::vector<Verification>> vWT = GetSidechainWTs(x);

        // Find the current best
        Verification best;
        best.nWorkScore = 0;
        for (size_t y = 0; y < vWT.size(); y++) {
            const Verification& v = vWT[y].back();
            if (best.nWorkScore == 0)
                best = v;
            if (v.nWorkScore > best.nWorkScore)
                best = v;
        }

        // Upvote the current best
        for (size_t z = 0; z < vWT.size(); z++) {
            const Verification& v = vWT[z].back();
            if (v.wtxid == best.wtxid) {
                os << 1;
            } else {
                os << 0;
            }
            // Delimit WT^(s)
            if (z != vWT.size() - 1)
                os << std::hex << '.';
        }

        // Delimit sidechains
        if (x != DB.size() - 1)
            os << std::hex << '|';
    }

    // Add the state stream to a script
    CScript script;
    std::string state(os.str());
    script << OP_RETURN << std::vector<unsigned char>(state.begin(), state.end());

    return script;
}

std::string SidechainDB::ToString() const
{
    if (!HasState())
        return "No sidechain WT^'s yet for this period.\n";

    std::stringstream ss;
    for (size_t x = 0; x < ARRAYLEN(ValidSidechains); x++) {
        ss << GetSidechainName(x) << std::endl;
        std::vector<std::vector<Verification>> vWTScores = GetSidechainWTs(x);

        // Print the current top WT^(s)
        for (size_t y = 0; y < vWTScores.size(); y++) {
            Verification v = vWTScores[y].back();
            ss << "WT[" << y << "]:\n" << v.ToString() << std::endl;
        }
    }
    return ss.str();
}

std::vector<std::vector<Verification>> SidechainDB::GetSidechainWTs(uint8_t nSidechain) const
{
    std::vector<std::vector<Verification>> vSidechainWT;

    if (!HasState())
        return vSidechainWT;

    for (size_t x = 0; x < DB[nSidechain].size(); x++) {
        const Verification &v = DB[nSidechain][x];

        bool match = false;
        for (size_t y = 0; y < vSidechainWT.size(); y++) {
            if (!vSidechainWT[y].size())
                continue;

            if (v.wtxid == vSidechainWT[y][0].wtxid) {
                vSidechainWT[y].push_back(v);
                match = true;
                break;
            }
        }
        if (!match) {
            std::vector<Verification> vWT;
            vWT.push_back(v);
            vSidechainWT.push_back(vWT);
        }
    }

    return vSidechainWT;
}

bool SidechainDB::IndexValid(uint8_t nSidechain) const
{
    if (!(nSidechain < ARRAYLEN(ValidSidechains)))
        return false;

    // Check that number coresponds to a valid sidechain
    switch (nSidechain) {
    case SIDECHAIN_TEST:
    case SIDECHAIN_HIVEMIND:
    case SIDECHAIN_WIMBLE:
        return true;
    default:
        return false;
    }
}

std::string SidechainDB::GetSidechainName(uint8_t nSidechain) const
{
    // Check that number coresponds to a valid sidechain
    switch (nSidechain) {
    case SIDECHAIN_TEST:
        return "SIDECHAIN_TEST";
    case SIDECHAIN_HIVEMIND:
        return "SIDECHAIN_HIVEMIND";
    case SIDECHAIN_WIMBLE:
        return "SIDECHAIN_WIMBLE";
    default:
        break;
    }
    return "SIDECHAIN_UNKNOWN";
}

bool SidechainDB::HasState() const
{
    if (DB.size() < ARRAYLEN(ValidSidechains))
        return false;

    if (!DB[SIDECHAIN_TEST].empty())
        return true;
    else
    if (!DB[SIDECHAIN_HIVEMIND].empty())
        return true;
    else
    if (!DB[SIDECHAIN_WIMBLE].empty())
        return true;

    return false;
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
