// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINCLIENT_H
#define SIDECHAINCLIENT_H

#include "primitives/sidechain.h"
#include "uint256.h"

#include <boost/property_tree/json_parser.hpp>

#include <vector>

class SidechainClient
{
public:
    SidechainClient();

    bool BroadcastWTJoin(std::string hex);

    std::vector<SidechainDeposit> UpdateDeposits(uint8_t nSidechain);

private:
    bool SendRequestToMainchain(const std::string json, boost::property_tree::ptree &ptree);
};

#endif // SIDECHAINCLIENT_H
