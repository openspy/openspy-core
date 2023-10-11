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
	FILE *fd = fopen(path.c_str(),"rb");
	if(fd) {
		while(1) {
			uint8_t ch;
			int len = fread(&ch, sizeof(uint8_t), 1, fd);
			if (len != sizeof(uint8_t)) break;
			ret += ch;
		}
		fclose(fd);
	}
	
	return ret;
}

void load_packages_data(UT::Config* cfg, std::vector<OS::ConfigNode> nodes) {
	std::vector<OS::ConfigNode>::iterator it = nodes.begin();
	while (it != nodes.end()) {
		UT::PackageItem item;

		OS::ConfigNode root = *it;
		std::vector<OS::ConfigNode> children = root.GetArrayChildren();
		std::vector<OS::ConfigNode>::iterator it2 = children.begin();
		while (it2 != children.end()) {

			OS::ConfigNode child = *it2;
			std::string key = child.GetKey();
			if (key.compare("guid") == 0) {
				item.guid = child.GetValue();
			}
			else if (key.compare("md5hash") == 0) {
				item.hash = child.GetValue();
			}
			else if (key.compare("version") == 0) {
				item.version = child.GetValueInt();
			}
			it2++;
		}
		cfg->packages.push_back(item);
		it++;
	}
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
			UT::Config *msConfig = new UT::Config();

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
				} else if(key.compare("latest-client-version") == 0) {
					msConfig->latest_client_version = atoi(configNode.GetValue().c_str());
				} else if (key.compare("packages") == 0) {
					load_packages_data(msConfig, configNode.GetArrayChildren());
				}
				it2++;
			}

			result.push_back(msConfig);
		}
		it++;
	}
	return result;
}

void idle_handler(uv_idle_t* handle) {
	g_gameserver->tick();
}

int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_idle_t idler;

	uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, idle_handler);

	OS::Init("utmaster", NULL);

	g_gameserver = new UT::Server();

	char address_buff[256];
	char port_buff[16];
	size_t temp_env_sz = sizeof(address_buff);

	if(uv_os_getenv("OPENSPY_UTMASTER_BIND_ADDR", (char *)&address_buff, &temp_env_sz) != UV_ENOENT) {
		temp_env_sz = sizeof(port_buff);
		uv_os_getenv("OPENSPY_UTMASTER_BIND_PORT", (char *)&port_buff, &temp_env_sz);
		uint16_t port = atoi(port_buff);

		UT::Driver *driver = new UT::Driver(g_gameserver, address_buff, port, false);

		char mapping_buff[256];
		temp_env_sz = sizeof(mapping_buff);
		uv_os_getenv("OPENSPY_UTMASTER_MAPPINGS_PATH", (char *)&mapping_buff, &temp_env_sz);

		std::vector<UT::Config *> configMapping = LoadConfigMapping(mapping_buff);
		driver->SetConfig(configMapping);


		OS::LogText(OS::ELogLevel_Info, "Adding UT Driver: %s:%d\n", address_buff, port);
		g_gameserver->addNetworkDriver(driver);
	}

	
    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);

    delete g_gameserver;

    OS::Shutdown();
	return 0;
}

void shutdown() {
    if(g_running) {
        g_running = false;
    }
}
