#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-flow-classifier.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiCsma1Persistent");

void experiment(uint32_t nNodes, uint32_t packetSize, bool verbose, bool pcap, 
                uint32_t simTime, uint32_t maxPackets, uint32_t interval, uint32_t serverNode)
{
  std::cout << "Running WiFi 1-persistent CSMA simulation with " << nNodes << " nodes (AP=" << serverNode << ")..." << std::endl;

  if (serverNode >= nNodes) {
    std::cerr << "ERROR: serverNode must be less than nNodes\n";
    exit(1);
  }

  GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("WifiCsma1Persistent", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(nNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.Set("Frequency", UintegerValue(2412)); // 2.4 GHz channel 1

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-1persistent");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer devices;
  for (uint32_t i = 0; i < nNodes; i++) {
    if (i == serverNode) {
      mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    }
    devices.Add(wifi.Install(phy, mac, nodes.Get(i)));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", 
                                "MinX", DoubleValue(0.0), 
                                "MinY", DoubleValue(0.0), 
                                "DeltaX", DoubleValue(10.0), 
                                "DeltaY", DoubleValue(10.0), 
                                "GridWidth", UintegerValue(5), 
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  Ptr<Node> apNode = nodes.Get(serverNode);
  Ptr<MobilityModel> apMobility = apNode->GetObject<MobilityModel>();
  apMobility->SetPosition(Vector(25.0, 25.0, 0.0));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper apServer(9);
  ApplicationContainer apServerApp = apServer.Install(apNode);
  apServerApp.Start(Seconds(1.0));
  apServerApp.Stop(Seconds(simTime));

  UdpEchoClientHelper clientToAp(interfaces.GetAddress(serverNode), 9);
  clientToAp.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  clientToAp.SetAttribute("Interval", TimeValue(MilliSeconds(interval)));
  clientToAp.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nNodes; i++) {
    if (i != serverNode) {
      ApplicationContainer app = clientToAp.Install(nodes.Get(i));
      app.Start(Seconds(2.0 + 0.1 * i));
      app.Stop(Seconds(simTime));
      clientApps.Add(app);
    }
  }

  UdpEchoServerHelper clientServer(10);
  ApplicationContainer clientServerApps;
  for (uint32_t i = 0; i < nNodes; i++) {
    if (i != serverNode) {
      clientServerApps.Add(clientServer.Install(nodes.Get(i)));
    }
  }
  clientServerApps.Start(Seconds(1.0));
  clientServerApps.Stop(Seconds(simTime));

  ApplicationContainer apClientApps;
  for (uint32_t i = 0; i < nNodes; i++) {
    if (i != serverNode) {
      UdpEchoClientHelper apToClient(interfaces.GetAddress(i), 10);
      apToClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
      apToClient.SetAttribute("Interval", TimeValue(MilliSeconds(interval)));
      apToClient.SetAttribute("PacketSize", UintegerValue(packetSize));
      ApplicationContainer app = apToClient.Install(apNode);
      app.Start(Seconds(3.0 + 0.1 * i));
      app.Stop(Seconds(simTime));
      apClientApps.Add(app);
    }
  }

  if (pcap) {
    phy.EnablePcap("wifi-ap-trace", devices.Get(serverNode));
  }

  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

  AnimationInterface anim("wifi-1persistent.xml");
  anim.EnablePacketMetadata(true);
  for (uint32_t i = 0; i < nNodes; i++) {
    if (i == serverNode) {
      anim.UpdateNodeColor(i, 255, 0, 0);
      anim.UpdateNodeDescription(i, "AP");
    } else {
      anim.UpdateNodeColor(i, 0, 0, 255);
      anim.UpdateNodeDescription(i, "Client");
    }
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

  for (auto iter = stats.begin(); iter != stats.end(); iter++) {
    if (classifier) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
      std::string src = (t.sourceAddress == interfaces.GetAddress(serverNode)) ? "AP" : "Client";
      std::string dst = (t.destinationAddress == interfaces.GetAddress(serverNode)) ? "AP" : "Client";
      NS_LOG_INFO("Flow " << iter->first << " (" << src << ":" << t.sourcePort << " -> " << dst << ":" << t.destinationPort << ")");
      NS_LOG_INFO("  Tx Packets: " << iter->second.txPackets);
      NS_LOG_INFO("  Rx Packets: " << iter->second.rxPackets);
      NS_LOG_INFO("  Lost Packets: " << iter->second.lostPackets);
      NS_LOG_INFO("  Throughput: " << iter->second.rxBytes * 8.0 / (simTime - 2) / 1000 << " Kbps");
    }
  }

  Simulator::Destroy();
  NS_LOG_INFO("Simulation completed");
}

int main(int argc, char *argv[])
{
  uint32_t nNodes = 25;
  uint32_t packetSize = 512;
  uint32_t simTime = 20;
  uint32_t maxPackets = 50;
  uint32_t interval = 100;
  uint32_t serverNode = 24;
  bool verbose = true;
  bool pcap = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nNodes", "Number of nodes", nNodes);
  cmd.AddValue("packetSize", "Packet size", packetSize);
  cmd.AddValue("simTime", "Simulation time", simTime);
  cmd.AddValue("maxPackets", "Maximum packets to send", maxPackets);
  cmd.AddValue("interval", "Interval between packets (ms)", interval);
  cmd.AddValue("serverNode", "Server (AP) node index", serverNode);
  cmd.AddValue("verbose", "Enable verbose output", verbose);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcap);
  cmd.Parse(argc, argv);

  experiment(nNodes, packetSize, verbose, pcap, simTime, maxPackets, interval, serverNode);

  return 0;
}
