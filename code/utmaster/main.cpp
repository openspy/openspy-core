#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include <OS/Config/AppConfig.h>
#include "server/UTServer.h"
#include "server/UTDriver.h"
INetServer *g_gameserver = NULL;
bool g_running = true;

void shutdown();

void on_exit(void) {
    shutdown();
}

void sig_handler(int signo)
{
    shutdown();
}

std::string get_file_contents(std::string path) {
	std::string ret;
	FILE *fd = fopen(path.c_str(),"r");
	if(fd) {
		fseek(fd,0,SEEK_END);
		int len = ftell(fd);
		fseek(fd,0,SEEK_SET);

		char *str_data = (char *)malloc(len+1);
		fread(str_data, len, 1, fd);
		str_data[len] = 0;
		ret = str_data;
		free((void *)str_data);
	}
	fclose(fd);
	return ret;
}

std::vector<UT::Config *> LoadConfigMapping(std::string filePath) {
	std::vector<UT::Config *> result;
	OS::Config *cfg = new OS::Config(filePath);

	OS::ConfigNode node;
	cfg->GetRootNode().FindObjectField("clientMappings", node);

	std::vector<OS::ConfigNode> children = node.GetArrayChildren();
	std::vector<OS::ConfigNode>::iterator it = children.begin();
	while(it != children.end()) {
		OS::ConfigNode child = *it;
		if (child.GetKey().compare("clientMapping") == 0) {
			UT::Config *msConfig = new UT::Config();;

			std::vector<OS::ConfigNode> mappingChildren = child.GetArrayChildren();
			std::vector<OS::ConfigNode>::iterator it2 = mappingChildren.begin();
			while(it2 != mappingChildren.end()) {
				OS::ConfigNode configNode = *it2;
				std::string key = configNode.GetKey();
				if(key.compare("clientname") == 0) {
					msConfig->clientName = configNode.GetValue();
				} else if(key.compare("gameid") == 0) {
					msConfig->gameid = configNode.GetValueInt();
				} else if(key.compare("server") == 0) {
					msConfig->is_server = configNode.GetValueInt() == 1;
				} else if(key.compare("motd") == 0) {
					msConfig->motd = get_file_contents(configNode.GetValue());
				}
				it2++;
			}

			result.push_back(msConfig);
		}
		it++;
	}
	return result;
}

#include "server/UTPeer.h"

void parse_servers() {

	uint8_t packet[] = { /* Packet 14 */

	/*0x05, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 
	0x01*/

	0x43, 0x00, 0x00, 0x00, 0x4a, 0x5b, 0x77, 0x7c, 
	0x0a, 0x1a, 0x0b, 0x1a, 0x1a, 0x44, 0x65, 0x61, 
	0x74, 0x68, 0x20, 0x57, 0x61, 0x72, 0x72, 0x61, 
	0x6e, 0x74, 0x3a, 0x4d, 0x6f, 0x6e, 0x73, 0x74, 
	0x65, 0x72, 0x20, 0x4d, 0x61, 0x73, 0x00, 0x0c, 
	0x44, 0x4d, 0x2d, 0x42, 0x75, 0x64, 0x2d, 0x48, 
	0x42, 0x2d, 0x44, 0x00, 0x09, 0x49, 0x6e, 0x76, 
	0x61, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x03, 0x14, 
	0x04, 0x00, 0x00, 0x00, 0x02, 0x31, 0x00, 0x48, 
	0x00, 0x00, 0x00, 0x4a, 0x5b, 0x77, 0x7c, 0x61, 
	0x1e, 0x62, 0x1e, 0x1a, 0x44, 0x65, 0x61, 0x74, 
	0x68, 0x20, 0x57, 0x61, 0x72, 0x72, 0x61, 0x6e, 
	0x74, 0x3a, 0x4d, 0x6f, 0x6e, 0x73, 0x74, 0x65, 
	0x72, 0x20, 0x4d, 0x61, 0x73, 0x00, 0x11, 0x44, 
	0x4d, 0x2d, 0x47, 0x6c, 0x61, 0x63, 0x69, 0x65, 
	0x72, 0x46, 0x61, 0x63, 0x69, 0x6c, 0x69, 0x00, 
	0x09, 0x49, 0x6e, 0x76, 0x61, 0x73, 0x69, 0x6f, 
	0x6e, 0x00, 0x01, 0x14, 0x04, 0x00, 0x00, 0x00, 
	0x02, 0x31, 0x00, 0x48, 0x00, 0x00, 0x00, 0xad, 
	0xe1, 0xb8, 0xc9, 0x61, 0x1e, 0x62, 0x1e, 0x1a, 
	0x4d, 0x6f, 0x6e, 0x73, 0x74, 0x65, 0x72, 0xa0, 
	0x4d, 0x61, 0x64, 0x6e, 0x65, 0x73, 0x73, 0x20, 
	0x2d, 0xa0, 0x52, 0x50, 0x47, 0xa0, 0x57, 0x6f, 
	0x50, 0x00, 0x11, 0x44, 0x4d, 0x2d, 0x54, 0x68, 
	0x65, 0x4c, 0x61, 0x73, 0x74, 0x53, 0x65, 0x6e, 
	0x74, 0x69, 0x6e, 0x00, 0x09, 0x49, 0x6e, 0x76, 
	0x61, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x04, 0x10, 
	0x04, 0x00, 0x00, 0x00, 0x02, 0x31, 0x00 };

	FILE *fd = fopen("buff.bin", "wb");
	fwrite(&packet, 1, sizeof(packet), fd);
	fclose(fd);

	OS::Buffer buffer;
	buffer.WriteBuffer(&packet, sizeof(packet));
	buffer.resetReadCursor();

	int num_servers = 3;
	for(int i=0;i<num_servers;i++) {
		uint32_t buff_len = buffer.ReadInt();
		int start = buffer.readRemaining();
		printf("buff len: %d\n", buff_len);
		uint32_t ip = buffer.ReadInt();	

		struct sockaddr_in addr;
		addr.sin_addr.s_addr = (ip);

		char ipinput[64];
		memset(&ipinput, 0, sizeof(ipinput));

		inet_ntop(AF_INET, &(addr.sin_addr), ipinput, sizeof(ipinput));

		uint16_t gameport = buffer.ReadShort();
		uint16_t queryport = buffer.ReadShort();

		printf("ip: %s %d %d\n",ipinput ,gameport, queryport);

		std::string hostname = UT::Peer::Read_FString(buffer);
		std::string level = UT::Peer::Read_FString(buffer);
		printf("info: %s - %s\n", hostname.c_str(), level.c_str());

		std::string group = UT::Peer::Read_FString(buffer);
		printf("group: %s\n", group.c_str());
		uint16_t unk1 = buffer.ReadShort();
		uint32_t unk2 = buffer.ReadInt();
		uint16_t unk3 = buffer.ReadShort();
		uint8_t unk4 = buffer.ReadByte();
		printf("%d %d %d %d\n", unk1, unk2, unk3, unk4);
		/*uint16_t unk1 = buffer.ReadShort();
		uint32_t unk2 = buffer.ReadInt();
		uint16_t unk3 = buffer.ReadShort();
		uint8_t unk4 = buffer.ReadByte();
		uint32_t unk5 = buffer.ReadInt();
		uint32_t unk6 = buffer.ReadInt();
		uint16_t unk7 = buffer.ReadShort();
		printf("unk: %02x %08x %d %d %d %d %d\n", unk1, unk2, unk3, unk4, unk5, unk6, unk7);*/

		printf("read: %d\n", start - buffer.readRemaining());
		break;

		//misc stuff - 9 bytes
		
	}



}
int main() {
	#ifndef _WIN32
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
	#else
		WSADATA wsdata;
		WSAStartup(MAKEWORD(1, 0), &wsdata);
	#endif

		OS::Config *cfg = new OS::Config("openspy.xml");
		AppConfig *app_config = new AppConfig(cfg, "utmaster");
		OS::Init("utmaster", app_config);

		g_gameserver = new UT::Server();


		std::vector<std::string> drivers = app_config->getDriverNames();
		std::vector<std::string>::iterator it = drivers.begin();
		while (it != drivers.end()) {
			std::string s = *it;
			bool proxyFlag = false;
			std::vector<OS::Address> addresses = app_config->GetDriverAddresses(s, proxyFlag);
			OS::Address address = addresses.front();
			std::string mappingLocation;
			app_config->GetVariableString(s, "client-mapping", mappingLocation);
			std::vector<UT::Config *> configMapping = LoadConfigMapping(mappingLocation);
			UT::Driver *driver = new UT::Driver(g_gameserver, address.ToString(true).c_str(), address.GetPort(), proxyFlag);
			driver->SetConfig(configMapping);
			OS::LogText(OS::ELogLevel_Info, "Adding utmaster Driver: %s:%d proxy: %d\n", address.ToString(true).c_str(), address.GetPort(), proxyFlag);
			g_gameserver->addNetworkDriver(driver);
			it++;
	}

	g_gameserver->init();
	while(g_running) {
		g_gameserver->tick();
	}

    delete g_gameserver;

    OS::Shutdown();
    return 0;
}

void shutdown() {
    if(g_running) {
        g_running = false;
    }
}