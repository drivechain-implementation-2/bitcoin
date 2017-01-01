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
    if (!SidechainNumberValid(nSidechain))
        return false;

    if (HaveWTCached(wtx.GetHash()))
        return false;

    if (vWTCache.size() >= SIDECHAIN_MAX_WT)
        return false;

    const Sidechain& s = ValidSidechains[nSidechain];
    uint16_t nTau = s.nWaitPeriod + s.nVerificationPeriod;

    if (Update(nSidechain, nTau, 0, wtx.GetHash())) {
        vWTCache.push_back(wtx);
        return true;
    }

    return false;
}

bool SidechainDB::AddSidechainDeposit(const CTransaction& tx)
{
    // Create sidechain deposit objects from transaction outputs
    std::vector<SidechainDeposit> vDepositAdd;
    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CScript &script = tx.vout[i].scriptPubKey;
        if (script.IsWorkScoreScript()) {

            uint8_t nSidechain = (unsigned int)*script.begin();
            if (!SidechainNumberValid(nSidechain))
                return false;

            std::vector<unsigned char> vch;
            opcodetype opcode;
            CScript::const_iterator pkey = script.begin() + 1;
            if (!script.GetOp2(pkey, opcode, &vch))
                return false;
            if (vch.size() != sizeof(uint160))
                return false;

            CKeyID keyID = CKeyID(uint160(vch));
            if (keyID.IsNull())
                return false;

            SidechainDeposit deposit;
            deposit.dtx = tx;
            deposit.keyID = keyID;
            deposit.nSidechain = nSidechain;

            vDepositAdd.push_back(deposit);
        }
    }

    // Go through the list of deposits, only add to the cache if ALL are valid
    bool error = false;
    for (const SidechainDeposit& d : vDepositAdd) {
        if (!SidechainNumberValid(d.nSidechain))
            error = true;

        if (d.keyID.IsNull())
            error = true;

        if (d.dtx.IsNull())
            error = true;

        if (!d.dtx.IsSidechainDeposit())
            error = true;
    }
    if (error)
        return false;

    for (const SidechainDeposit& d : vDepositAdd) {
        if (!HaveDepositCached(d))
            vDepositCache.push_back(d);
    }

    return true;
}

bool SidechainDB::Update(const CScript& script)
{
    // If there aren't any known WT^(s) there is nothing to update
    if (!HasState())
        return false;

    // State script cannot be smaller than this
    if (script.size() < 5)
        return false;

    CScript::const_iterator pc = script.begin();
    std::vector<unsigned char> vch;
    opcodetype opcode;

    // Get the opcode
    if (!script.GetOp2(pc, opcode, &vch))
        return false;
    if (opcode != OP_RETURN)
        return false;

    if (script.size() < 3)
        return false;
    if (script[1] != SCOP_VERSION || script[2] != SCOP_VERSION_DELIM)
        return false;

    uint8_t nSidechain = 0;
    size_t nWTIndex = 0;
    for (size_t i = 3; i < script.size(); i++) {
        if (!SidechainNumberValid(nSidechain))
            return false;
        if (script[i] == SCOP_WT_DELIM) {
            nWTIndex++;
            continue;
        }

        if (script[i] == SCOP_SC_DELIM) {
            nWTIndex = 0;
            nSidechain++;
            continue;
        }

        // Check for valid vote type before going further
        const char& vote = script[i];
        if (vote != SCOP_REJECT && vote != SCOP_VERIFY && vote != SCOP_IGNORE)
            continue;

        std::vector<std::vector<SidechainVerification>> vWT = GetSidechainWTs(nSidechain);
        if (nWTIndex > vWT.size())
            return false;

        const SidechainVerification& old = vWT[nWTIndex].back();

        // TODO mind limits
        uint16_t nBlocksLeft = old.nBlocksLeft - 1;
        if (vote == SCOP_REJECT)
            return Update(old.nSidechain, nBlocksLeft, old.nWorkScore - 1, old.wtxid);
        else
        if (vote == SCOP_VERIFY)
            return Update(old.nSidechain, nBlocksLeft, old.nWorkScore + 1, old.wtxid);
        else
        if (vote == SCOP_IGNORE)
            return Update(old.nSidechain, nBlocksLeft, old.nWorkScore, old.wtxid);
    }

    return false;
}

bool SidechainDB::Update(uint8_t nSidechain, uint16_t nBlocks, uint16_t nScore, uint256 wtxid)
{
    if (!SidechainNumberValid(nSidechain))
        return false;

    SidechainVerification v;
    v.nBlocksLeft = nBlocks;
    v.nSidechain = nSidechain;
    v.nWorkScore = nScore;
    v.wtxid = wtxid;

    DB[nSidechain].push_back(v);

    return true;
}

bool SidechainDB::CheckWorkScore(uint8_t nSidechain, uint256 wtxid) const
{
    if (!SidechainNumberValid(nSidechain))
        return false;

    // Get the scores for this sidechain (x = WT^, y = verifications)
    std::vector<std::vector<SidechainVerification>> vScores;
    vScores = GetSidechainWTs(nSidechain);

    for (size_t x = 0; x < vScores.size(); x++) {
        if (!vScores[x].size())
            continue;
        if (vScores[x][0].wtxid != wtxid)
            continue;

        uint32_t nScore = 0;
        for (size_t y = 0; y < vScores[x].size(); y++) {
            if (std::abs(vScores[x][y].nWorkScore - nScore) > 1)
                continue;
            nScore = vScores[x][y].nWorkScore;
        }

        if (nScore >= ValidSidechains[nSidechain].nMinWorkScore)
            return true;
    }

    return false;
}

CTransaction SidechainDB::GetWT(uint8_t nSidechain) const
{
    if (!SidechainNumberValid(nSidechain))
        return CTransaction();

    // Grab list of sidechain's WT^ candidates
    std::vector<std::vector<SidechainVerification>> vWT;
    vWT = GetSidechainWTs(nSidechain);

    // Lookup the current best WT^ candidate for sidechain
    uint256 hashBest;
    int scoreBest = 0;
    for (size_t x = 0; x < vWT.size(); x++) {
        if (!vWT[x].size())
            continue;
        if (vWT[x].back().nWorkScore > scoreBest) {
            hashBest = vWT[x].back().wtxid;
            scoreBest = vWT[x].back().nWorkScore;
        }
    }

    // Did we find anything?
    if (hashBest == uint256())
        return CTransaction();

    // Is the WT^ verified?
    if (scoreBest < ValidSidechains[nSidechain].nMinWorkScore)
        return CTransaction();

    // Return the full transaction from WT^ cache if it exists
    for (const CTransaction& tx : vWTCache) {
        if (tx.GetHash() == hashBest)
            return tx;
    }

    return CTransaction();
}

bool SidechainDB::HaveWTCached(uint256 wtxid) const
{
    for (const CTransaction& tx : vWTCache) {
        if (tx.GetHash() == wtxid)
            return true;
    }
    return false;
}

bool SidechainDB::HaveDepositCached(SidechainDeposit deposit) const
{
    for (const SidechainDeposit& d : vDepositCache) {
        if (d == deposit)
            return true;
    }
    return false;
}

std::vector<SidechainDeposit> SidechainDB::GetSidechainDeposits(uint8_t nSidechain) const
{
    std::vector<SidechainDeposit> vSidechainDeposit;

    for (size_t i = 0; i < vDepositCache.size(); i++) {
        if (vDepositCache[i].nSidechain == nSidechain)
            vSidechainDeposit.push_back(vDepositCache[i]);
    }

    return vSidechainDeposit;
}

CScript SidechainDB::CreateStateScript() const
{
    /* TODO
     * use merged mining to decide what the new state is.
     * For now, just upvoting the current best WT^.
     */
    if (!HasState())
        return CScript();

    CScript script;
    script << OP_RETURN << SCOP_VERSION << SCOP_VERSION_DELIM;

    // Add the current DB state to the stream
    for (size_t x = 0; x < DB.size(); x++) {
        std::vector<std::vector<SidechainVerification>> vWT = GetSidechainWTs(x);

        // Find the current best
        SidechainVerification best;
        best.nWorkScore = 0;
        for (size_t y = 0; y < vWT.size(); y++) {
            const SidechainVerification& v = vWT[y].back();
            if (best.nWorkScore == 0)
                best = v;
            if (v.nWorkScore > best.nWorkScore)
                best = v;
        }

        // Upvote the current best
        for (size_t z = 0; z < vWT.size(); z++) {
            const SidechainVerification& v = vWT[z].back();
            if (v.wtxid == best.wtxid) {
                script << SCOP_VERIFY;
            } else {
                script << SCOP_REJECT;
            }
            // Delimit WT^(s)
            if (z != vWT.size() - 1)
                script << SCOP_WT_DELIM;
        }

        // Delimit sidechains
        if (x != DB.size() - 1)
            script << SCOP_SC_DELIM;
    }
    return script;
}

std::string SidechainDB::ToString() const
{
    std::stringstream ss;

    if (!HasState())
        return "No sidechain WT^'s yet for this period.\n";

    for (const Sidechain& s : ValidSidechains) {
        // Add sidechain's human readable name to stream
        ss << s.GetSidechainName() << std::endl;

        // Collect the current most verified WT^ for each sidechain
        std::vector<std::vector<SidechainVerification>> vWTCandidate;
        vWTCandidate = GetSidechainWTs(s.nSidechain);

        // Add current top WT^ for this sidechain to stream
        for (size_t i = 0; i < vWTCandidate.size(); i++) {
            const SidechainVerification& v = vWTCandidate[i].back();
            ss << "WT[" << i << "]:\n" << v.ToString() << std::endl;
        }
    }

    return ss.str();
}

std::vector<std::vector<SidechainVerification>> SidechainDB::GetSidechainWTs(uint8_t nSidechain) const
{
    std::vector<std::vector<SidechainVerification>> vSidechainWT;

    if (!HasState())
        return vSidechainWT;

    for (size_t x = 0; x < DB[nSidechain].size(); x++) {
        const SidechainVerification &v = DB[nSidechain][x];

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
            std::vector<SidechainVerification> vWT;
            vWT.push_back(v);
            vSidechainWT.push_back(vWT);
        }
    }

    return vSidechainWT;
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

bool SidechainNumberValid(uint8_t nSidechain)
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

bool SidechainDeposit::operator==(const SidechainDeposit& a) const
{
    return (a.nSidechain == nSidechain &&
            a.keyID == keyID &&
            a.dtx == dtx);
}

std::string Sidechain::GetSidechainName() const
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

std::string Sidechain::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nWaitPeriod=" << nWaitPeriod << std::endl;
    ss << "nVerificationPeriod=" << nVerificationPeriod << std::endl;
    ss << "nMinWorkScore=" << nMinWorkScore << std::endl;
    return ss.str();
}

std::string SidechainDeposit::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "keyID=" << keyID.ToString()<< std::endl;
    ss << "dtx=" << dtx.ToString() << std::endl;
    return ss.str();
}

std::string SidechainVerification::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nBlocksLeft=" << nBlocksLeft << std::endl;
    ss << "nWorkScore=" << nWorkScore << std::endl;
    ss << "wtxid=" << wtxid.ToString() << std::endl;
    return ss.str();
}
