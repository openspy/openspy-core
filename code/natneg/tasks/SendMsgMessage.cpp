#include <server/NNServer.h>
#include <server/NNDriver.h>
#include <server/NATMapper.h>
#include <algorithm>

#include <sstream>

#include "tasks.h"

#include <jansson.h>

namespace NN {
	/*
	{"type":"connect","to_address":"127.0.0.1:51652",
	"data":
	[
		{"from_address":"127.0.0.1:58435","driver_address":"0.0.0.0:27901","hostname":"OS-dev","gamename":"gmtest","version":4,"type":"init","cookie":666,
		
		"data":{"porttype":0,"clientindex":0,"usegameport":1,"private_address":"127.0.1.1:0"}}
		,{"from_address":"127.0.0.1:52566","driver_address":"0.0.0.0:27901","hostname":"OS-dev","gamename":"gmtest","version":4,"type":"init","cookie":666,
		
		"data":{"porttype":1,"clientindex":0,"usegameport":1,"private_address":"127.0.1.1:0"}}
		,{"from_address":"127.0.0.1:52566","driver_address":"0.0.0.0:27901","hostname":"OS-dev","gamename":"gmtest","version":4,"type":"init","cookie":666,
		
		"data":{"porttype":2,"clientindex":0,"usegameport":1,"private_address":"127.0.1.1:58435"}}
		,{"from_address":"127.0.0.1:52566","driver_address":"0.0.0.0:27901","hostname":"OS-dev","gamename":"gmtest","version":4,"type":"init","cookie":666,
		
		"data":{"porttype":3,"clientindex":0,"usegameport":1,"private_address":"127.0.1.1:58435"}}]}
	*/
	ConnectionSummary LoadSummaryFromJson(json_t *data) {
		json_t *j = NULL;
		json_t *first_item = json_array_get(data, 0);
		json_t *last_item;
		ConnectionSummary summary;

		j = json_object_get(first_item, "cookie");
		summary.cookie = json_integer_value(j);

		j = json_object_get(first_item, "version");
		summary.version = json_integer_value(j);

		summary.address_hits = json_array_size(data);
		summary.required_addresses = summary.address_hits; //XXX: remove this
		last_item = json_array_get(data, summary.address_hits - 1);

		j = json_object_get(first_item, "usegameport");
		summary.usegameport = json_integer_value(j);

		j = json_object_get(last_item, "data");
		j = json_object_get(j, "private_address");
		summary.private_address = OS::Address(json_string_value(j));

		for(int i=0;i<summary.address_hits;i++) {
			j = json_array_get(data, i);
			if(j != NULL) {
				j = json_object_get(j, "from_address");
				summary.m_port_type_addresses[i] = OS::Address(json_string_value(j));
			}
		}

		return summary;
	}
	void Handle_ConnectPacket(json_t *root, NN::Driver *driver) {
		json_t *to_address_json = json_object_get(root, "to_address");
		OS::Address to_address(json_string_value(to_address_json));

		

		json_t *data = json_object_get(root, "data");
		json_t *error = json_object_get(data, "finished");

		NatNegPacket packet;
		memcpy(&packet.magic, NNMagicData, NATNEG_MAGIC_LEN);
		
		packet.packettype = NN_CONNECT;

		if(error != NULL) {
			json_t *j = json_object_get(data, "cookie");
			if(j != NULL) {
				packet.cookie = htonl(json_integer_value(j));
			} else {
				packet.cookie = 0;
			}
				
			j = json_object_get(root, "version");
			packet.version = json_integer_value(j);
			packet.Packet.Connect.finished = json_integer_value(error);
			packet.Packet.Connect.gotyourdata = 0;
			packet.Packet.Connect.remoteIP = 0;
			packet.Packet.Connect.remotePort = 0;
			driver->SendPacket(to_address, &packet);
			OS::LogText(OS::ELogLevel_Info, "[%s] Send Connection Error: %d %d", to_address.ToString().c_str(), packet.Packet.Connect.finished, ntohl(packet.cookie));
		} else {
			ConnectionSummary summary = LoadSummaryFromJson(data);

			NAT nat;
			OS::Address next_public_address, next_private_address, connect_address;
			NN::LoadSummaryIntoNAT(summary, nat);
			NN::DetermineNatType(nat);
			NN::DetermineNextAddress(nat, next_public_address, next_private_address);


			if(next_public_address.ip == to_address.ip && summary.version != 1) {
				connect_address = next_private_address;
				if(connect_address.GetPort() == 0) {
					connect_address.port = next_public_address.port;
				}
			} else {
				connect_address = next_public_address;
			}
			packet.version = summary.version;
			packet.cookie = htonl(summary.cookie);
			packet.Packet.Connect.remoteIP = connect_address.GetIP();
			packet.Packet.Connect.remotePort = htons(connect_address.GetPort());

			packet.Packet.Connect.finished = 0;
			packet.Packet.Connect.gotyourdata = 0;

			driver->SendPacket(to_address, &packet);
			OS::LogText(OS::ELogLevel_Info, "[%s] Connect Packet (to: %s) - %d", to_address.ToString().c_str(), connect_address.ToString().c_str(), summary.cookie);
		}
	}
	bool Handle_HandleRecvMessage(TaskShared::TaskThreadData *thread_data, std::string message) {
		NN::Server *server = (NN::Server*)uv_loop_get_data(uv_default_loop());
		json_t *root = json_loads(message.c_str(), 0, NULL);
		if(!root) return false;

		json_t *type = json_object_get(root, "type");
		if(type) {
			const char *type_str = json_string_value(type);
			json_t *driver_address_json = json_object_get(root, "driver_address");

			json_t *hostname_json = json_object_get(root, "hostname");

			bool should_process = true;

			if(hostname_json && json_is_string(hostname_json)) {
				const char *hostname = json_string_value(hostname_json);
				if(stricmp(OS::g_hostName, hostname) != 0) {
					should_process = false;
				}
			}

			if(should_process) {
				OS::Address driver_address;
				driver_address = OS::Address(json_string_value(driver_address_json));
				

				NN::Driver *driver = server->findDriverByAddress(driver_address);
				
				if(stricmp(type_str, "connect") == 0) {
					Handle_ConnectPacket(root, driver);
				}
			}
		}

		json_decref(root);
		return true;
	}
}