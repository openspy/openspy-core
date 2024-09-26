#include <server/NNServer.h>
#include <server/NNDriver.h>
#include <server/NATMapper.h>
#include <algorithm>

#include <sstream>

#include "tasks.h"

#include <jansson.h>

namespace NN {
	bool read_amqp_message(json_t *root, NatNegPacket *packet) {
		json_t *obj = NULL;
		switch(packet->packettype) {
			case NN_ERTTEST:
			case NN_INITACK:
				obj = json_object_get(root, "porttype");
				packet->Packet.Init.porttype = json_integer_value(obj);

				obj = json_object_get(root, "client_index");
				packet->Packet.Init.clientindex = json_integer_value(obj);

				obj = json_object_get(root, "use_gameport");
				packet->Packet.Init.usegameport = obj == json_true() || json_integer_value(obj);

				obj = json_object_get(root, "localip");
				packet->Packet.Init.localip = json_integer_value(obj);

				obj = json_object_get(root, "localport");
				packet->Packet.Init.localport = json_integer_value(obj);
				
			break;
			case NN_CONNECT:
				obj = json_object_get(root, "remote_ip");
				packet->Packet.Connect.remoteIP = json_integer_value(obj);

				obj = json_object_get(root, "remote_port");
				packet->Packet.Connect.remotePort = htons(json_integer_value(obj));

				obj = json_object_get(root, "got_your_data");
				packet->Packet.Connect.gotyourdata = json_integer_value(obj);

				obj = json_object_get(root, "finished");
				packet->Packet.Connect.finished = json_integer_value(obj);
			break;
		}
		return true;
	}
	bool Handle_HandleRecvMessage(TaskShared::TaskThreadData *thread_data, std::string message) {
		NN::Server *server = (NN::Server*)uv_loop_get_data(uv_default_loop());
		json_t *root = json_loads(message.c_str(), 0, NULL);
		if(!root) return false;

		json_t *type = json_object_get(root, "type");
		uint8_t packet_type = NN_INITACK;
		if(type) {
			json_t *gamename_obj = json_object_get(root, "gamename");
			std::string gamename;
			if(gamename_obj) {
				gamename = json_string_value(gamename_obj);
			}
			const char *type_str = json_string_value(type);
			json_t *driver_address_json = json_object_get(root, "driver_address");

			json_t *version_obj = json_object_get(root, "version");
			json_t *data_obj = json_object_get(root, "data");
			json_t *cookie_obj = json_object_get(root, "cookie");
			if (version_obj != NULL) {
				int cookie = json_integer_value(cookie_obj);
				int version = json_integer_value(version_obj);
				OS::Address driver_address;
				driver_address = OS::Address(json_string_value(driver_address_json));
			
				OS::Address to_address;
				driver_address_json = json_object_get(root, "address");
				to_address = OS::Address(json_string_value(driver_address_json));


				NN::Driver *driver = server->findDriverByAddress(driver_address);
				if(!driver) {
					return true;
				}
				NatNegPacket nn_packet;
				if(stricmp(type_str, "connect") == 0) {
					packet_type = NN_CONNECT;
				} else if (stricmp(type_str, "ert") == 0) {
					packet_type = NN_ERTTEST;
				} else if (stricmp(type_str, "preinit_ack") == 0) { 
					packet_type = NN_PREINIT_ACK;
				} else if (stricmp(type_str, "init_ack") == 0) { 
					packet_type = NN_INITACK;
				} else if (stricmp(type_str, "report_ack") == 0) { 
					packet_type = NN_REPORT_ACK;
				}
				nn_packet.packettype = packet_type;
				nn_packet.version = version;
				nn_packet.cookie = htonl(cookie);

				read_amqp_message(data_obj, &nn_packet);
				
				switch(packet_type) {
					case NN_CONNECT:
					driver->send_connect(to_address, &nn_packet);
					break;
					case NN_ERTTEST:
					driver->send_ert_test(to_address, &nn_packet);
					break;
					case NN_PREINIT_ACK:
					driver->send_preinit_ack(to_address, &nn_packet);
					break;
					case NN_INITACK:
					driver->send_init_ack(to_address, &nn_packet, gamename);
					break;
					case NN_REPORT_ACK:
					driver->send_report_ack(to_address, &nn_packet);
					break;
				}
			}
		}

		json_decref(root);
		return true;
	}
}