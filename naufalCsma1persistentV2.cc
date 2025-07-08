#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include <cstdio>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FinalProject");

void experiment(uint32_t nNodes, uint32_t packetSize, bool verbose, bool pcap, 
                uint32_t simTime, uint32_t maxPackets, uint32_t interval, uint32_t serverNode)
{
  std::cout << "Running simulation with " << nNodes << " nodes (AP=" << serverNode << ")..." << std::endl;

  // Validasi serverNode
  if (serverNode >= nNodes) {
    std::cerr << "ERROR: serverNode (" << serverNode 
              << ") must be less than nNodes (" << nNodes << ")\n";
    exit(1);
  }

  // ======== PERBAIKAN 1: KONFIGURASI ARP ========
  GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));
  Config::SetDefault("ns3::ArpCache::AliveTimeout", TimeValue(Seconds(3600)));
  Config::SetDefault("ns3::ArpCache::DeadTimeout", TimeValue(Seconds(3600)));

  if (verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("FinalProject", LOG_LEVEL_INFO);
  }

  // Setup CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(10)));

  // Buat node jaringan
  NodeContainer nodes;
  nodes.Create(nNodes);

  // Instal perangkat CSMA
  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  // ======== PERBAIKAN 2: INISIALISASI MAC ADDRESS ========
  for (uint32_t i = 0; i < devices.GetN(); i++) {
    Ptr<CsmaNetDevice> device = DynamicCast<CsmaNetDevice>(devices.Get(i));
    device->SetAddress(Mac48Address::Allocate());
  }

  // Setup mobilitas
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
  
  // Pindahkan AP
  Ptr<Node> apNode = nodes.Get(serverNode);
  Ptr<MobilityModel> apMobility = apNode->GetObject<MobilityModel>();
  apMobility->SetPosition(Vector(25.0, 25.0, 0.0));

  // Instal stack internet
  InternetStackHelper stack;
  stack.Install(nodes);

  // Atur alamat IP
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // ... [kode aplikasi UDP tetap sama] ...
  // ========================= TRAFFIC CLIENT -> AP =========================
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

  // ========================= TRAFFIC AP -> CLIENT =========================
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

  // Enable PCAP tracing
  if (pcap) {
    csma.EnablePcap("ap-trace", devices.Get(serverNode), true);
  }

  // Setup FlowMonitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

  // Setup animasi NetAnim
  AnimationInterface anim("scratch/naufalCsma/animation.xml");
  anim.EnablePacketMetadata(true);  // Tampilkan info paket
  
  // Warna node
  for (uint32_t i = 0; i < nNodes; i++) {
    if (i == serverNode) {
      anim.UpdateNodeColor(i, 255, 0, 0);  // AP merah
      anim.UpdateNodeDescription(i, "AP");
      anim.UpdateNodeSize(i, 10, 10);
    } else {
      anim.UpdateNodeColor(i, 0, 0, 255);  // Client biru
      anim.UpdateNodeDescription(i, "Client");
    }
  }

  // Jalankan simulasi
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Analisis hasil FlowMonitor
  flowMonitor->CheckForLostPackets();
  
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  
  NS_LOG_INFO("\nFlow monitor results:");
  for (auto iter = stats.begin(); iter != stats.end(); iter++) {
    if (classifier) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
      
      std::string src = (t.sourceAddress == interfaces.GetAddress(serverNode)) ? "AP" : "Client";
      std::string dst = (t.destinationAddress == interfaces.GetAddress(serverNode)) ? "AP" : "Client";
      
      NS_LOG_INFO("Flow " << iter->first << " (" << src << ":" << t.sourcePort 
                  << " -> " << dst << ":" << t.destinationPort << ")");
      NS_LOG_INFO("  Tx Packets: " << iter->second.txPackets);
      NS_LOG_INFO("  Rx Packets: " << iter->second.rxPackets);
      NS_LOG_INFO("  Lost Packets: " << iter->second.lostPackets);
      NS_LOG_INFO("  Throughput: " << iter->second.rxBytes * 8.0 / (simTime - 2) / 1000 << " Kbps");
    }
  }

  flowMonitor->SerializeToXmlFile("scratch/naufalCsma/flow-stats.xml", true, true);
  Simulator::Destroy();
  NS_LOG_INFO("Simulation completed");
}

int main(int argc, char *argv[])
{
  uint32_t nNodes = 25;
  uint32_t packetSize = 512;
  uint32_t simTime = 20;
  uint32_t maxPackets = 50;
  uint32_t interval = 100; // ms
  uint32_t serverNode = 24;
  bool verbose = true;
  bool pcap = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nNodes", "Number of nodes/devices", nNodes);
  cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
  cmd.AddValue("maxPackets", "Max packets to send", maxPackets);
  cmd.AddValue("interval", "Interval between packets (ms)", interval);
  cmd.AddValue("serverNode", "Node ID for AP", serverNode);
  cmd.AddValue("verbose", "Enable logging", verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", pcap);
  cmd.Parse(argc, argv);

  if (nNodes < 10) {
    std::cout << "WARNING: Minimum 10 nodes recommended\n";
    nNodes = 10;
  }

  serverNode = (serverNode < nNodes) ? serverNode : nNodes - 1;

  experiment(nNodes, packetSize, verbose, pcap, simTime, maxPackets, interval, serverNode);
  
  // Run Python analysis
  int status = system("python3 scratch/naufalCsma/maintesv2_analyze.py");
  if (status != 0)
  {
    std::cerr << "⚠️  Gagal menjalankan maintesv1_analyze.py" << std::endl;
  }
  
  return 0;
}


