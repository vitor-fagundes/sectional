#include "ns3/core-module.h"
#include "ns3/node-container.h"
#include "ns3/csma-helper.h"
#include "ns3/lr-wpan-helper.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/lr-wpan-spectrum-value-helper.h"
#include "ns3/spectrum-value.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/sixlowpan-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/mobility-module.h"
#include "sys/stat.h"
#include <iostream>
#include <fstream>

#include "NodeAPApplication.h"
#include "NodeApplication.h"
#include "capabilities.h"
#include "constants.h"
#include "task.h"

#define SIMTIME 900.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Contaski_V1");

void generateCap(int);
void generateTasks(int);
double generateStartTimeDelay();

int main (int argc, char *argv[]){
	NS_LOG_UNCOND ("Contaski_V1");
	LogComponentEnable("Contaski_V1", LOG_LEVEL_ALL);
	LogComponentEnable("Contaski_V1_AP", LOG_LEVEL_ALL);
	LogComponentEnable("Contaski_V1_Nodes", LOG_LEVEL_ALL);

	uint32_t nNodes = 3;
	int run = 0;

	CommandLine cmd;
	cmd.AddValue ("nNodes", "Number of node devices", nNodes);
	cmd.AddValue ("run", "Run number", run);
	cmd.Parse (argc,argv);

	NS_LOG_INFO("Creating " << nNodes << " nodes");
	NodeContainer nodes;
	nodes.Create(nNodes);

	NodeContainer apContainer;
	apContainer.Create(1);

	NodeContainer allNodes(nodes, apContainer);

	NS_LOG_INFO("Creating internet stack");
	InternetStackHelper internetv6;
	internetv6.SetIpv4StackInstall(false);
	internetv6.SetIpv6StackInstall(true);
	internetv6.Install(allNodes);

	NS_LOG_INFO ("Create channels");
    /*CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue (5000000));
    csma.SetChannelAttribute("Delay", TimeValue (MilliSeconds (2)));
    NetDeviceContainer netdevices = csma.Install(allNodes);*/
	LrWpanHelper lrwpan(false);
	NetDeviceContainer netdevices = lrwpan.Install(allNodes);
	lrwpan.AssociateToPan(netdevices, 0);
	//lrwpan.EnablePcapAll("contaski-");

	NS_LOG_INFO("Creating sixlowpan");
	SixLowPanHelper sixlowpan;
   	//sixlowpan.SetDeviceAttribute("ForceEtherType", BooleanValue (true) );
   	NetDeviceContainer six1 = sixlowpan.Install(netdevices);

	NS_LOG_INFO ("Create networks and assign IPv6 Addresses");
   	Ipv6AddressHelper ipv6;
  	ipv6.SetBase (Ipv6Address ("2020:1::"), Ipv6Prefix (64));
  	Ipv6InterfaceContainer i1 = ipv6.Assign(six1);

	NS_LOG_INFO("Setting up mobility");
	MobilityHelper mobility;
	/*mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0|Max=200]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0|Max=200]")
								);*/
	double squarePerNode = ceil(sqrt(40000/nNodes));
	double nodesPerLine = ceil(200/squarePerNode);
	mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
            					"MinX", DoubleValue (0.0),
								"MinY", DoubleValue (0.0),
								"DeltaX", DoubleValue (squarePerNode),
								"DeltaY", DoubleValue (squarePerNode),
             					"GridWidth", UintegerValue ( nodesPerLine ),
								"LayoutType", StringValue ("RowFirst"));
  	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  	mobility.Install(nodes);
	
	MobilityHelper apMobilityHelper;
	apMobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	apMobilityHelper.Install(apContainer);
	Ptr<ConstantPositionMobilityModel> apMobility = apContainer.Get(0)->GetObject<ConstantPositionMobilityModel>();
	apMobility->SetPosition(Vector(100, 100, 0.0));

	NS_LOG_INFO("Create applications");
	Ptr<nr2::NodeAPApplication> apApplication = Create<nr2::NodeAPApplication>();
	Ptr<Node> ap = apContainer.Get(0);
	ap->AddApplication(apApplication);

	Ptr<LrWpanNetDevice> apnetdev = DynamicCast<LrWpanNetDevice>(ap->GetDevice(1));
	auto apphy = apnetdev->GetPhy();
	LrWpanSpectrumValueHelper svh;
	Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity (9, 11); //Range of 200m according to lr-wpan-error-distance-plot
	apphy->SetTxPowerSpectralDensity(psd);

	generateTasks(nNodes);

	stringstream st;
	st << "tasksFile-" << nNodes << ".txt";
	string nameTF = st.str();
	
	ifstream taskFile(nameTF);
	string taskLine;
	nr2::taskVector* tasks = new nr2::taskVector();
	while(getline(taskFile, taskLine)){
		tasks->push_back( new nr2::Task(taskLine) );
	}

	apApplication->setTasks(tasks);
	apApplication->setup();
	apApplication->SetStartTime(Seconds(10.0));
	apApplication->SetStopTime(Seconds(SIMTIME+10.0));

	auto nodeAddrs = new std::vector<Ipv6Address>;

	generateCap(nNodes);

	stringstream ss;
	ss << "capacitiesFile-" << nNodes << "-contaski.txt";
	string name = ss.str();
	
	ifstream capFile(name);
	string line;
	for (size_t i = 0; i < nodes.GetN() && getline(capFile, line); i++){
		Ptr<nr2::NodeApplication> nodeApplication = Create<nr2::NodeApplication>();
		double delay = generateStartTimeDelay();
		nodeApplication->SetStartTime(Seconds(0.0 + delay));
		nodeApplication->SetStopTime(Seconds(SIMTIME));
		nodeApplication->setDelay(delay);
		nodes.Get(i)->AddApplication(nodeApplication);

		nodeApplication->setup( *nr2::parseCapabilities(line) );
		nodeAddrs->push_back(nodeApplication->GetNodeIpAddress());

		Ptr<LrWpanNetDevice> nodenetdev = DynamicCast<LrWpanNetDevice>(nodes.Get(i)->GetDevice(1));
		auto phy = nodenetdev->GetPhy();
		LrWpanSpectrumValueHelper svh;
		Ptr<SpectrumValue> psd = svh.CreateTxPowerSpectralDensity (-10, 11); //Range of 50m according to lr-wpan-error-distance-plot
		phy->SetTxPowerSpectralDensity(psd);
	}

	auto apAddress = apContainer.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress();
	NS_LOG_INFO("AP: " << apAddress);
	for (size_t i = 0; i < nodes.GetN(); i++){
		Ptr<nr2::NodeApplication> nodeapp = nodes.Get(i)->GetApplication(0)->GetObject<nr2::NodeApplication>();
		nodeapp->setAPAddress(apAddress);
		nodeapp->setAllNodesAddrs(*nodeAddrs);
	}

	for (size_t i = 0; i < nodes.GetN(); i++){
		Ptr<MobilityModel> deviceMobility = nodes.Get(i)->GetObject<MobilityModel>();
		double distance = deviceMobility->GetDistanceFrom(apMobility);

		Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
        Ipv6InterfaceAddress iaddr = ipv6->GetAddress(1, 0);
        Ipv6Address ipAddr = iaddr.GetAddress();

		NS_LOG_INFO("N: IP " << ipAddr << " D " << distance);
	}

	/*stringstream sp;
	sp << "contaski-" << nNodes << "-" << run;
	string prefix = sp.str();
	csma.EnablePcap(prefix, netdevices.Get(0), true);*/
	
	NS_LOG_INFO("Starting Simulation");	
	Simulator::Stop( Seconds(SIMTIME+20.0) );
	Simulator::Run();
	
	Simulator::Destroy();

	NS_LOG_INFO("Simulation end");
}

void generateCap(int nNodes){
	struct stat buffer;
	stringstream ss;
	ss << "capacitiesFile-" << nNodes << "-contaski.txt";
	string name = ss.str();

	if(stat (name.c_str(), &buffer) == 0)
		return;

	ofstream capFile(name);

	std::random_device rd;
	std::default_random_engine generator{rd()};
	std::uniform_int_distribution<int> distribution(3, 6);

	nr2::capabilitiesVector* cap;

	for(int i = 0; i < nNodes; i++){
		cap = new nr2::capabilitiesVector(nr2::basicCapabilities);

		//int capSize = distribution(generator);
		for(int j = 3; j < 7; j++){
			cap->push_back( static_cast<nr2::capabilities>( distribution(generator) ) );
		}

		std::sort(cap->begin(), cap->end());
		auto last = std::unique(cap->begin(), cap->end());
		cap->erase(last, cap->end());

		capFile << nr2::serializeCapabilities(cap) << "\n";

		cap = nullptr;
	}

	capFile.close();
}

void generateTasks(int nNodes){
	struct stat buffer;
	stringstream ss;
	ss << "tasksFile-" << nNodes << ".txt";
	string name = ss.str();

	if(stat (name.c_str(), &buffer) == 0)
		return;

	ofstream tasksFile(name);

	for(int i = 0; i < 12; i++){
		auto taskUnit = new nr2::Task();

		tasksFile << taskUnit->serialize() << "\n";

		taskUnit = nullptr;
	}

	tasksFile.close();
}

double generateStartTimeDelay(){
	Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
	x->SetAttribute ("Min", DoubleValue (0.0));
	x->SetAttribute ("Max", DoubleValue (60.0));

	return x->GetValue();
}
