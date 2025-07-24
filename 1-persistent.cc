#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include <cstdio>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OnePersistentWifi2_4GHz");

void experiment(uint32_t nNodes, uint32_t packetSize, bool verbose, bool pcap,
                uint32_t simTime, uint32_t maxPackets, uint32_t interval, uint32_t serverNode,
                std::string dataRate, std::string phyDelay)
{
  std::cout << "Simulasi dengan " << nNodes << " node menggunakan WiFi 2.4GHz...\n";


   // ======== PERBAIKAN 1: KONFIGURASI ARP ========
  GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));
  Config::SetDefault("ns3::ArpCache::AliveTimeout", TimeValue(Seconds(3600)));
  Config::SetDefault("ns3::ArpCache::DeadTimeout", TimeValue(Seconds(3600)));

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("FinalProject", LOG_LEVEL_INFO);
  }
   // Buat node jaringan
  NodeContainer nodes;
  nodes.Create(nNodes);

    // ======== PERBAIKAN 2: INISIALISASI MAC ADDRESS ========
  for (uint32_t i = 0; i < devices.GetN(); i++) {
    Ptr<CsmaNetDevice> device = DynamicCast<CsmaNetDevice>(devices.Get(i));
    device->SetAddress(Mac48Address::Allocate());
  }

  // Posisi node
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

  // PHY + Channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("ErpOfdmRate6Mbps"),
                               "ControlMode", StringValue("ErpOfdmRate6Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  NetDeviceContainer devices;
  for (uint32_t i = 0; i < nNodes; ++i) {
    if (i == serverNode) {
      mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    } else {
      mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    }
    devices.Add(wifi.Install(phy, mac, nodes.Get(i)));
  }

   // Pindahkan AP
  Ptr<Node> apNode = nodes.Get(serverNode);
  Ptr<MobilityModel> apMobility = apNode->GetObject<MobilityModel>();
  apMobility->SetPosition(Vector(25.0, 25.0, 0.0));

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

 // Atur alamat IP
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Server AP
  Ptr<Node> apNode = nodes.Get(serverNode);
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simTime));

  // Client ke AP
  for (uint32_t i = 0; i < nNodes; i++) {
    if (i == serverNode) continue;

    UdpEchoClientHelper client(interfaces.GetAddress(serverNode), 9);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps = client.Install(nodes.Get(i));
    apps.Start(Seconds(2.0 + 0.1 * i));
    apps.Stop(Seconds(simTime));
  }

  // Monitoring
  if (pcap) {
    phy.EnablePcap("wifi-ap", devices.Get(serverNode));
  }
 // Setup FlowMonitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> monitor = flowHelper.InstallAll();
 // Setup FlowMonitor
  AnimationInterface anim("scratch/naufalCsma/animation.xml");
  anim.EnablePacketMetadata(true);// Tampilkan info paket

  for (uint32_t i = 0; i < nNodes; i++) {
    anim.UpdateNodeDescription(i, (i == serverNode) ? "AP" : "Client");
    anim.UpdateNodeColor(i, (i == serverNode) ? 255 : 0, 0, (i == serverNode) ? 0 : 255);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
    std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << ":\n";
    std::cout << "  Tx Packets: " << iter->second.txPackets << ", Rx Packets: " << iter->second.rxPackets << "\n";
    std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (simTime - 2) / 1000 << " Kbps\n";
    
  flowMonitor->SerializeToXmlFile("scratch/naufalCsma/flow-stats.xml", true, true);
  Simulator::Destroy();
  NS_LOG_INFO("Simulation completed");
  }

  Simulator::Destroy();
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
  std::string dataRate = "6Mbps"; //ukuran bandwith
  std::string phyDelay = "10us"; //delay dalam micro second

  CommandLine cmd(__FILE__);
  cmd.AddValue("nNodes", "Number of nodes", nNodes);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("simTime", "Simulation time", simTime);
  cmd.AddValue("maxPackets", "Max packets to send", maxPackets);
  cmd.AddValue("interval", "Interval between packets (ms)", interval);
  cmd.AddValue("serverNode", "Node ID to act as AP", serverNode);
  cmd.AddValue("verbose", "Enable logging", verbose);
  cmd.AddValue("pcap", "Enable pcap", pcap);
  cmd.AddValue("dataRate", "Data rate", dataRate);
  cmd.AddValue("phyDelay", "Channel delay", phyDelay);
  cmd.Parse(argc, argv);

   if (nNodes < 10) {
    std::cout << "WARNING: Minimum 10 nodes recommended\n";
    nNodes = 10;
  }
     if (nNodes > 80) {
    std::cout << "WARNING: Maximum 80 nodes recommended\n";
    nNodes = 80;
  }

  serverNode = (serverNode < nNodes) ? serverNode : nNodes - 1;

  experiment(nNodes, packetSize, verbose, pcap, simTime, maxPackets, interval, serverNode, dataRate, phyDelay);
 
 // Run Python analysis
  int status = system("python3 scratch/naufalCsma/maintesv2_analyze.py");
  if (status != 0)
  {
    std::cerr << "⚠️  Gagal menjalankan maintesv1_analyze.py" << std::endl;
  }
  return 0;
}
