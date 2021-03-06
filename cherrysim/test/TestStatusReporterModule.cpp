////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH.
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "Utility.h"
#include "CherrySimTester.h"
#include "CherrySimUtils.h"
#include "Logger.h"
#include <json.hpp>

using json = nlohmann::json;

TEST(TestStatusReporterModule, TestCommands) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;

    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(100 * 1000);

    tester.sim->FindNodeById(1)->gs.logger.EnableTag("DEBUGMOD");
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("DEBUGMOD");

    tester.SendTerminalCommand(1, "action 2 status get_status");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"status\",\"module\":3");


    tester.SendTerminalCommand(1, "action 2 status get_device_info");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"device_info\",\"module\":3,");

    tester.SendTerminalCommand(1, "action 2 status get_connections");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"connections\",\"nodeId\":2,\"module\":3,\"partners\":[");

    tester.SendTerminalCommand(1, "action 2 status get_nearby");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"nearby_nodes\",\"module\":3,\"nodes\":[");

    tester.SendTerminalCommand(1, "action 2 status set_init");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"set_init_result\",\"nodeId\":2,\"module\":3}");

    //tester.SendTerminalCommand(1, "action 2 status keep_alive"); //TODO: Hard to test!
    //tester.SimulateUntilMessageReceived(10 * 1000, 1, "TODO"); 

    tester.SendTerminalCommand(1, "action 2 status get_errors");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"error_log_entry\",\"nodeId\":2,\"module\":3,");

    tester.SendTerminalCommand(1, "action 2 status livereports 42");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "LiveReporting is now 42");

    tester.SendTerminalCommand(1, "action 2 status get_rebootreason");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"reboot_reason\",\"nodeId\":2,\"module\":3,");
}

TEST(TestStatusReporterModule, TestPeriodicTimeSend) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    //testerConfig.verbose = true;

    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1 });
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1 });
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(10 * 1000);
    tester.sim->FindNodeById(2)->gs.logger.EnableTag("STATUSMOD");

    //Send a write command
    tester.SendTerminalCommand(1, "component_act 2 3 1 0xABCD 0x1234 01 13");
    tester.SimulateUntilMessageReceived(10 * 1000, 2, "Periodic Time Send is now: 1");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"nodeId\":2,\"type\":\"component_sense\",\"module\":3,\"requestHandle\":13,\"actionType\":2,\"component\":\"0xABCD\",\"register\":\"0x1234\",\"payload\":");
    
    //Make sure that the status module automatically disables the periodic time send after 10 minutes.
    tester.SimulateUntilMessageReceived(10 * 60 * 1000, 2, "Periodic Time Send is now: 0");
}

#ifndef GITHUB_RELEASE
TEST(TestStatusReporterModule, TestHopsToSinkFixing) {
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.terminalId = 0;
    simConfig.SetToPerfectConditions();
    //testerConfig.verbose = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 5});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(1000 * 1000);

    for (int i = 1; i <= 6; i++) tester.sim->FindNodeById(i)->gs.logger.EnableTag("DEBUGMOD");

    NodeIndexSetter setter(1);
    MeshConnections inConnections = tester.sim->FindNodeById(2)->gs.cm.GetMeshConnections(ConnectionDirection::DIRECTION_IN);
    MeshConnections outConnections = tester.sim->FindNodeById(2)->gs.cm.GetMeshConnections(ConnectionDirection::DIRECTION_OUT);
    u16 invalidHops = tester.sim->GetTotalNodes() + 10; // a random value that is not possible to be correct
    u16 validHops   = tester.sim->GetTotalNodes() -  1;    // initialize to max number of hops

    // set all inConnections for node 2 to invalid and find the one with least hops to sink
    for (int i = 0; i < inConnections.count; i++) {
        NodeIndexSetter setter(1);
        u16 tempHops;
        tempHops = inConnections.handles[i].GetHopsToSink();
        if (tempHops < validHops) validHops = tempHops;
        inConnections.handles[i].SetHopsToSink(invalidHops);
    }

    // set all outConnections for node 2 to invalid and find the one with least hops to sink
    for (int i = 0; i < outConnections.count; i++) {
        NodeIndexSetter setter(1);
        outConnections.handles[i].SetHopsToSink(invalidHops);
    }

    tester.SendTerminalCommand(1, "action max_hops status keep_alive");
    tester.SimulateForGivenTime(1000 * 10);

    // get_erros will collect errors from the node but will also clear them
    tester.SendTerminalCommand(1, "action 2 status get_errors");
    // This must not check for the exact number in "extra" as during meshing and simulation, a different invalid amount of hops may be recorded.
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"error_log_entry\",\"nodeId\":2,\"module\":3,\"errType\":2,\"code\":44,\"extra\":");

    tester.SendTerminalCommand(1, "action max_hops status keep_alive");
    tester.SimulateForGivenTime(1000 * 10);
    
    tester.SendTerminalCommand(1, "action 2 status get_errors");
    tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"error_log_entry\",\"nodeId\":2,\"module\":3,");

    // We expect that incorrect hops error wont be received as hopsToSink should have been fixed together with first keep_alive message.
    {
        Exceptions::DisableDebugBreakOnException disabler;
        ASSERT_THROW(tester.SimulateUntilMessageReceived(10 * 1000, 1, "{\"type\":\"error_log_entry\",\"nodeId\":2,\"module\":3,\"errType\":%u,\"code\":%u", LoggingError::CUSTOM, CustomErrorTypes::FATAL_INCORRECT_HOPS_TO_SINK), TimeoutException);
    }
}
#endif //GITHUB_RELEASE

#ifndef GITHUB_RELEASE
TEST(TestStatusReporterModule, TestConnectionRssiReportingWithoutNoise) {
    //Test Rssi reporting when RssiNoise is disabled 
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.rssiNoise = false;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(10 * 1000);

    tester.SimulateGivenNumberOfSteps(100);

    int rssiCalculated = (int)tester.sim->GetReceptionRssi(tester.sim->FindNodeById(1), tester.sim->FindNodeById(2));

    tester.SendTerminalCommand(1, "action 1 status get_connections");

    //Wait for the message of reported rssi
    std::vector<SimulationMessage> message = {
        SimulationMessage(1,"{\"type\":\"connections\",\"nodeId\":1,\"module\":3,\"partners\":[")
    };

    tester.SimulateUntilMessagesReceived(10 * 100, message);

    const std::string messageComplete = message[0].GetCompleteMessage();

    //parse rssi value
    auto j = json::parse(messageComplete);
    int rssisReported[4];
    rssisReported[0] = j["/rssiValues/0"_json_pointer].get<int>();
    rssisReported[1] = j["/rssiValues/1"_json_pointer].get<int>();
    rssisReported[2] = j["/rssiValues/2"_json_pointer].get<int>();
    rssisReported[3] = j["/rssiValues/3"_json_pointer].get<int>();

    int rssiReported = 0;
    for (int i = 0; i < 4; i++)
    {
        if (rssisReported[i] != 0)
        {
            rssiReported = rssisReported[i];
        }
    }

    /*Check if the reported RSSI is equal to calculated one when rssi noise is inactive*/
    if (rssiReported != rssiCalculated) {
        FAIL() << "RSSI calculated is not equal to RSSI reported";
    }
}
#endif //GITHUB_RELEASE

#ifndef GITHUB_RELEASE
TEST(TestStatusReporterModule, TestConnectionRssiReportingWithNoise) {
    //Test Rssi reporting when RssiNoise is disabled 
    CherrySimTesterConfig testerConfig = CherrySimTester::CreateDefaultTesterConfiguration();
    //testerConfig.verbose = true;
    SimConfiguration simConfig = CherrySimTester::CreateDefaultSimConfiguration();
    simConfig.rssiNoise = true;
    simConfig.nodeConfigName.insert({ "prod_sink_nrf52", 1});
    simConfig.nodeConfigName.insert({ "prod_mesh_nrf52", 1});
    CherrySimTester tester = CherrySimTester(testerConfig, simConfig);
    tester.Start();

    tester.SimulateUntilClusteringDone(10 * 1000);

    tester.SimulateGivenNumberOfSteps(100);

    int rssiCalculated = (int)tester.sim->GetReceptionRssi(tester.sim->FindNodeById(1), tester.sim->FindNodeById(2));

    tester.SendTerminalCommand(1, "action 1 status get_connections");

    //Wait for the message of reported rssi
    std::vector<SimulationMessage> message = {
        SimulationMessage(1,"{\"type\":\"connections\",\"nodeId\":1,\"module\":3,\"partners\":[")
    };

    tester.SimulateUntilMessagesReceived(10 * 100, message);

    const std::string messageComplete = message[0].GetCompleteMessage();

    //parse rssi value
    auto j = json::parse(messageComplete);
    int rssisReported[4];
    rssisReported[0] = j["/rssiValues/0"_json_pointer].get<int>();
    rssisReported[1] = j["/rssiValues/1"_json_pointer].get<int>();
    rssisReported[2] = j["/rssiValues/2"_json_pointer].get<int>();
    rssisReported[3] = j["/rssiValues/3"_json_pointer].get<int>();

    int rssiReported = 0;
    for (int i = 0; i < 4; i++)
    {
        if (rssisReported[i] != 0)
        {
            rssiReported = rssisReported[i];
        }
    }

    /*Check if the reported RSSI is equal to calculated one when rssi noise is inactive*/
    if (std::abs(rssiReported - rssiCalculated) > 15) {
        FAIL() << "RSSI calculated is not nearly equal to RSSI reported";
    }
}
#endif //GITHUB_RELEASE
