#include "./vanet.h"

#include "ns3/animation-interface.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;


uint32_t SentPackets = 0;
uint32_t ReceivedPackets = 0;
uint32_t LostPackets = 0;

void  ScheduleIpv4RoutingUpdates(){
    std::cout<<"Routing table recomputed\n";
Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
    Simulator::Schedule(Seconds(0.1),&ScheduleIpv4RoutingUpdates);
}

void ScheduleVehicleReg() {
    int cnt = 0;
    for (uint32_t i = 0; i < numVehicle; i++)
    {
        Ptr<VehicleApp> vehicleApp = DynamicCast<VehicleApp>(vehicles.Get(i)->GetApplication(0));
        if(!vehicleApp->reg) {
            Simulator::ScheduleWithContext(vehicleApp->GetNode()->GetId(),Seconds(0.1),
                            &VehicleApp::RegisterWithTA,
                            vehicleApp);
            cnt++;
        }
        
    }
    // std::cout << cnt << std::endl;
    if(cnt) Simulator::Schedule(Seconds(10.0), ScheduleVehicleReg);
}

void ScheduleRSUReg() {
    int cnt = 0;
    for (uint32_t i = 0; i < numRSU; i++)
    {
        Ptr<RSUApp> rsuApp = DynamicCast<RSUApp>(rsus.Get(i)->GetApplication(0));
        if(!rsuApp->registered) {
            Simulator::ScheduleWithContext(rsuApp->GetNode()->GetId(),Seconds(0.1),
                            &RSUApp::RegisterWithTA,
                            rsuApp);
            cnt++;
        }
        
    }
    // std::cout << cnt << std::endl;
    if(cnt) Simulator::Schedule(Seconds(10.0), ScheduleRSUReg);
}

void ScheduleLoginAndAuth(){
    int cnt = 0;
    for (uint32_t i = 0; i < numVehicle; i++)
    {
        Ptr<VehicleApp> vehicleApp = DynamicCast<VehicleApp>(vehicles.Get(i)->GetApplication(0));
        if(!vehicleApp->loggedIn) {
            Simulator::ScheduleWithContext(vehicleApp->GetNode()->GetId(),Seconds(0.1),
                            &VehicleApp::LoginandAuth,
                            vehicleApp);
           cnt++;
        }
        
    }
    // std::cout << "Login and auth completed for : "<<cnt << "vehicles" <<std::endl;
    if(numVehicle - cnt >= 0 && cnt!=0) {
        std::cout<<"Simulated again\n";
        Simulator::Schedule(Seconds(10.0), ScheduleLoginAndAuth);}
}

int
main(int argc, char* argv[])
{
    // RngSeedManager::SetSeed(generateRandom());
    std::string phyMode("OfdmRate6Mbps");
    double propLossFreq = 5.9e9;
    double heightZ = 10.0;

    vehicles.Create(numVehicle);
    rsus.Create(numRSU);
    tas.Create(1);

    // Install mobility on vehicle and RSU nodes
    MobilityHelper vehicleMobility;
    vehicleMobility.SetPositionAllocator(
        "ns3::RandomRectanglePositionAllocator",
        "X",
        StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
        "Y",
        StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));
    vehicleMobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                                     "Speed",
                                     StringValue("ns3::ConstantRandomVariable[Constant=20.0]"),
                                     "Bounds",
                                     StringValue("0|1000|0|1000"));
    vehicleMobility.Install(vehicles);

    MobilityHelper rsuMobility;
    rsuMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX",
                                     DoubleValue(0.0),
                                     "MinY",
                                     DoubleValue(0.0),
                                     "DeltaX",
                                     DoubleValue(250.0),
                                     "DeltaY",
                                     DoubleValue(250.0),
                                     "GridWidth",
                                     UintegerValue(5),
                                     "LayoutType",
                                     StringValue("RowFirst"));
    rsuMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    rsuMobility.Install(rsus);
    Ptr<ConstantPositionMobilityModel> position = CreateObject<ConstantPositionMobilityModel>();
    position->SetPosition(Vector(500, 500, 0));
    tas.Get(0)->AggregateObject(position);

    // Configure channel for communication in VANET
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel",
                                   "Frequency",
                                   DoubleValue(propLossFreq),
                                   "HeightAboveZ",
                                   DoubleValue(heightZ));
    Ptr<YansWifiChannel> channel = wifiChannel.Create();

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(channel);
    wifiPhy.Set("TxGain", DoubleValue(5.0));
    wifiPhy.Set("RxGain", DoubleValue(5.0));
    wifiPhy.Set("TxPowerStart", DoubleValue(50.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(50.0));

    NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();

    wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode",
                                       StringValue(phyMode),
                                       "ControlMode",
                                       StringValue(phyMode));

    NetDeviceContainer vehicleDevices = wifi80211p.Install(wifiPhy, wifi80211pMac, vehicles);
    NetDeviceContainer rsuDevices = wifi80211p.Install(wifiPhy, wifi80211pMac, rsus);
    //NetDeviceContainer taDevices = wifi80211p.Install(wifiPhy, wifi80211pMac, tas);

    AodvHelper aodv;
    // uint32_t maxPacket = 256;
    // aodv.Set("MaxQueueTime", TimeValue(Seconds(6.0)));
    // aodv.Set("MaxQueueLen", UintegerValue(maxPacket));

    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);
    InternetStackHelper stack;
    stack.SetRoutingHelper(list);

    stack.Install(vehicles);
    stack.Install(rsus);
 

    Ipv4AddressHelper addressHelper;
    // vehicleAddressHelper.SetBase("10.1.1.0", "255.255.255.0");
    // RSUAddressHelper.SetBase("10.2.1.0", "255.255.255.0");
    // TAAddressHelper.SetBase("10.3.1.0", "255.255.255.0");
    addressHelper.SetBase("10.1.1.0","255.255.255.0");
    // vehicleInterfaces = vehicleAddressHelper.Assign(vehicleDevices);
    // rsuInterfaces = vehicleAddressHelper.Assign(rsuDevices);
    // taInterfaces = vehicleAddressHelper.Assign(taDevices);
    vehicleInterfaces = addressHelper.Assign(vehicleDevices);
    rsuInterfaces = addressHelper.Assign(rsuDevices);

    NodeContainer lan(tas.Get(0),rsus);
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    // csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6)));
    NetDeviceContainer lanDevices = csma.Install(lan);
    stack.Install(tas);
    addressHelper.SetBase("172.16.1.0", "255.255.255.0");
    taInterfaces = addressHelper.Assign(lanDevices);
    
    std::cout<<"TA address is"<<taInterfaces.GetAddress(0)<<"\n";

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
 //ScheduleIpv4RoutingUpdates();

    Ptr<TAApp> centralApp = CreateObject<TAApp>();
    tas.Get(0)->AddApplication(centralApp);
    //ta->AddApplication(centralApp);
    centralApp->SetStartTime(Seconds(1.0));
    centralApp->SetStopTime(Seconds(150.0));

    for (uint32_t i = 0; i < numRSU; ++i)
    {
        Ptr<RSUApp> rsuApp = CreateObject<RSUApp>();
        rsuApp->SetRSU_SID(i);
        rsus.Get(i)->AddApplication(rsuApp);
        rsuApp->SetStartTime(Seconds(1.0));
        rsuApp->SetStopTime(Seconds(150.0));
    }

    for (uint32_t i = 0; i < numVehicle; ++i)
    {
        Ptr<VehicleApp> vehicleApp = CreateObject<VehicleApp>();
        vehicleApp->SetVehicleID(i);
        vehicles.Get(i)->AddApplication(vehicleApp);
        vehicleApp->SetStartTime(Seconds(1.0));
        vehicleApp->SetStopTime(Seconds(150.0));
    }

    std::cout << "LOG: Network Setup completed\n";
    std::cout << "Address of TA : " << tas.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal()
              << "\n";
    Simulator::Schedule(Seconds(5.0), ScheduleVehicleReg);
    Simulator::Schedule(Seconds(50.0), ScheduleRSUReg);
    Simulator::Schedule(Seconds(100.0), ScheduleLoginAndAuth);
    // for (uint32_t i = 0; i < numVehicle; i++)
    // {
    //     Ptr<VehicleApp> vehicleApp = DynamicCast<VehicleApp>(vehicles.Get(i)->GetApplication(0));
    //     Simulator::ScheduleWithContext(vehicleApp->GetNode()->GetId(),Seconds(2.0),
    //                         &VehicleApp::RegisterWithTA,
    //                         vehicleApp);
    // }

    // for (uint32_t i = 0; i < numRSU; i++)
    // {
    //     Ptr<RSUApp> rsuApp = DynamicCast<RSUApp>(rsus.Get(i)->GetApplication(0));
    //     Simulator::ScheduleWithContext(rsuApp->GetNode()->GetId(),Seconds(5.0),
    //                         &RSUApp::RegisterWithTA,
    //                         rsuApp);
    // }
    

    Simulator::Stop(Seconds(150.0));

    // AnimationInterface anim("vanet.xml");
    // anim.EnablePacketMetadata();
    // anim.EnableIpv4RouteTracking("vanet-routing-data.xml", Seconds(0), Seconds(20), Seconds(0.5));
   
    //csma.EnablePcap("rsulan",);
    FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Run();
    std::cout << "Simulation Started...\n";

int j=0;
float AvgThroughput = 0;
Time Jitter;
Time Delay;

Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
    {
	//   Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);

// NS_LOG_UNCOND("----Flow ID:" <<iter->first);
// NS_LOG_UNCOND("Src Addr" <<t.sourceAddress << "Dst Addr "<< t.destinationAddress);
// NS_LOG_UNCOND("Sent Packets=" <<iter->second.txPackets);
// NS_LOG_UNCOND("Received Packets =" <<iter->second.rxPackets);
// NS_LOG_UNCOND("Lost Packets =" <<iter->second.txPackets-iter->second.rxPackets);
// NS_LOG_UNCOND("Packet delivery ratio =" <<iter->second.rxPackets*100/iter->second.txPackets << "%");
// NS_LOG_UNCOND("Packet loss ratio =" << (iter->second.txPackets-iter->second.rxPackets)*100/iter->second.txPackets << "%");
// NS_LOG_UNCOND("Delay =" <<iter->second.delaySum);
// NS_LOG_UNCOND("Jitter =" <<iter->second.jitterSum);
// NS_LOG_UNCOND("Throughput =" <<iter->second.rxBytes * 8.0/(iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds())/1024<<"Kbps");

SentPackets = SentPackets +(iter->second.txPackets);
ReceivedPackets = ReceivedPackets + (iter->second.rxPackets);
LostPackets = LostPackets + (iter->second.txPackets-iter->second.rxPackets);
AvgThroughput = AvgThroughput + (iter->second.rxBytes * 8.0/(iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds())/1024);
Delay = Delay + (iter->second.delaySum);
Jitter = Jitter + (iter->second.jitterSum);

j = j + 1;

}

AvgThroughput = AvgThroughput/j;
NS_LOG_UNCOND("--------Total Results of the simulation----------"<<std::endl);
NS_LOG_UNCOND("Total sent packets  =" << SentPackets);
NS_LOG_UNCOND("Total Received Packets =" << ReceivedPackets);
NS_LOG_UNCOND("Total Lost Packets =" << LostPackets);
NS_LOG_UNCOND("Packet Loss ratio =" << ((LostPackets*100)/SentPackets)<< "%");
NS_LOG_UNCOND("Packet delivery ratio =" << ((ReceivedPackets*100)/SentPackets)<< "%");
NS_LOG_UNCOND("Average Throughput =" << AvgThroughput<< "Kbps");
NS_LOG_UNCOND("End to End Delay =" << Delay / ReceivedPackets);
NS_LOG_UNCOND("End to End Jitter delay =" << Jitter);
NS_LOG_UNCOND("Total Flod id " << j);
monitor->SerializeToXmlFile("vanet.xml", true, true);

    // Cleanup
    std::cout << "Average login time" << tim / numVehicle << "\n";
    std::cout << "Simulation Ended...\n";


    return 0;
}
