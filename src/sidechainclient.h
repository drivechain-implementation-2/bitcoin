// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINCLIENT_H
#define SIDECHAINCLIENT_H

#include "sidechaindb.h"
#include "uint256.h"

#include <boost/property_tree/json_parser.hpp>

#include <vector>

class SidechainClient
{
public:
    SidechainClient();

    bool sendWT(uint256 wtjid, std::string hex);

    std::vector<SidechainDeposit> getDeposits(uint256 sidechainid, uint32_t height);

private:
    bool sendRequestToMainchain(const std::string json, boost::property_tree::ptree &ptree);
};

#endif // SIDECHAINCLIENT_H
