#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include <server/QRServer.h>
#include <server/QRDriver.h>
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
#include <qr/tasks/tasks.h>
#include <tasks/tasks.h>
#include <server/v2.h>
namespace QR {
    void Driver::on_v2_heartbeat_processed(MM::MMTaskResponse response) {
		if(response.error_message != NULL) {
			response.driver->send_v2_error(response.from_address, response.v2_instance_key, 1, response.error_message);		
			return;
		} else if(response.challenge.length() != 0) {
			OS::Buffer buffer;		
			buffer.WriteByte(QR_MAGIC_1);
			buffer.WriteByte(QR_MAGIC_2);
			buffer.WriteByte(PACKET_CHALLENGE);
			buffer.WriteInt(response.v2_instance_key);
			buffer.WriteNTS(response.challenge);
			response.driver->SendPacket(response.from_address, buffer);
		}
    }
    void Driver::handle_v2_heartbeat(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer) {
        uint32_t i = 0;
        MM::ServerInfo server_info;
		server_info.m_address = from_address;

		std::string key, value;

		std::stringstream ss;

		while(true) {

			if(i%2 == 0) {
				key = buffer.ReadNTS();
				if (key.length() == 0) break;
			} else {
				value = buffer.ReadNTS();
				ss << "(" << key << "," << value << ") ";
			}

			if(value.length() > 0) {
				if(server_info.m_keys.find(key) == server_info.m_keys.end()) {
					server_info.m_keys[key] = value;
				}
				value = std::string();
			}
			i++;
		}

		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", from_address.ToString().c_str(), ss.str().c_str());
		ss.str("");


		uint16_t num_values = 0;

		while((num_values = htons(buffer.ReadShort()))) {
			std::vector<std::string> nameValueList;
			if(buffer.readRemaining() <= 3) {
				break;
			}
			uint32_t num_keys = 0;
			std::string x;
			while(buffer.readRemaining() && (x = buffer.ReadNTS()).length() > 0) {
				nameValueList.push_back(x);
				num_keys++;
			}
			unsigned int player=0/*,num_keys_t = num_keys*/,num_values_t = num_values*num_keys;
			i = 0;

			while(num_values_t--) {
				std::string name = nameValueList.at(i);

				x = buffer.ReadNTS();

				if(MM::isTeamString(name.c_str())) {
					if(server_info.m_team_keys[name].size() <= player) {
						server_info.m_team_keys[name].push_back(x);
					}
					else {
						server_info.m_team_keys[name][player] = x;
					}
					ss << "T(" << player << ") (" << name.c_str() << "," << x << ") ";
				} else {
					if(server_info.m_player_keys[name].size() <= player) {
						server_info.m_player_keys[name].push_back(x);
					} else {
						server_info.m_player_keys[name][player] = x;
					}
					ss << "P(" << player << ") (" << name.c_str() << "," << x << " ) ";
				}
				i++;
				if(i >= num_keys) {
					player++;
					i = 0;
				}
			}
		}


		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", from_address.ToString().c_str(), ss.str().c_str());
		ss.str("");

        TaskScheduler<MM::MMPushRequest, TaskThreadData> *scheduler = ((QR::Server *)(getServer()))->getScheduler();
        MM::MMPushRequest req;        

        req.from_address = from_address;
        req.v2_instance_key = *(uint32_t *)instance_key;
        req.driver = this;
        req.server = server_info;
        req.version = 2;
        req.type = MM::EMMPushRequestType_Heartbeat;
        req.callback = on_v2_heartbeat_processed;
        scheduler->AddRequest(req.type, req);
    }
}