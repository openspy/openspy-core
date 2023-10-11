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
    void Driver::on_v1_heartbeat_processed(MM::MMTaskResponse response) {
		if(response.error_message != NULL) {
			response.driver->send_v1_error(response.from_address, response.error_message);		
			return;
		} else if(response.challenge.length() != 0) {
            std::stringstream ss;
            ss << "\\secure\\" << response.challenge;

            std::string message = ss.str();

			OS::Buffer buffer;
            buffer.WriteBuffer(message.c_str(),message.length());
            response.driver->SendUDPPacket(response.query_address, buffer);  //some games like roguespaer require it to be sent to the query address (most games will accept this still)
            if(response.from_address != response.query_address) {
                response.driver->SendUDPPacket(response.from_address, buffer);  //other games require it to be sent to the from address (thps4ps2)
            }
            
		} else {
            response.driver->perform_v1_key_scan(response.query_address); //some games like roguespaer require it to be sent to the query address (most games will accept this still)
            if(response.from_address != response.query_address) {
                response.driver->perform_v1_key_scan(response.from_address); //other games require it to be sent to the from address (thps4ps2)
            }
        }
    }
    void Driver::handle_v1_heartbeat(OS::Address from_address, OS::KVReader reader) {
        MM::ServerInfo server_info;

        OS::Address query_address = OS::Address(from_address.GetIP(), reader.GetValueInt("heartbeat"));
        if(query_address.GetPort() == 0) {
            query_address.port = from_address.port;
        }
        server_info.m_address = query_address;

        std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> it_header = reader.GetHead();
        std::vector<std::pair< std::string, std::string> >::const_iterator it = it_header.first;
        it++; //skip heartbeat setting

        std::stringstream ss;

        while(it != it_header.second) {
            std::pair< std::string, std::string> p = *it;
            server_info.m_keys[p.first] = p.second;
            ss << "(" << p.first << "," << p.second << ") ";
            it++;
        }
		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", from_address.ToString().c_str(), ss.str().c_str());
		ss.str("");
        
        MM::MMPushRequest req;        

        req.from_address = from_address;
        req.v2_instance_key = 0;
        req.driver = this;
        req.server = server_info;
        req.version = 1;
        req.type = MM::EMMPushRequestType_Heartbeat;
        req.callback = on_v1_heartbeat_processed;
        AddRequest(req);
    }


    void Driver::on_v1_heartbeat_data_processed(MM::MMTaskResponse response) {
        if(response.error_message != NULL) {
			response.driver->send_v1_error(response.from_address, response.error_message);		
			return;
		}
    }

    void Driver::handle_v1_heartbeat_data(OS::Address from_address, OS::KVReader reader) {
        MM::ServerInfo server_info;
        server_info.m_address = from_address;

        std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> it_header = reader.GetHead();
        std::vector<std::pair< std::string, std::string> >::const_iterator it = it_header.first;

        std::stringstream ss;

        while(it != it_header.second) {
            std::pair< std::string, std::string> p = *it;
            int player_id = 0;
            std::string variable_name = p.first;
            if(MM::isPlayerString(p.first, variable_name, player_id)) {
                if((int)server_info.m_player_keys[variable_name].size() <= player_id) {
                    server_info.m_player_keys[variable_name].push_back(p.second);
                } else {
                    server_info.m_player_keys[variable_name][player_id] = p.second;
                }
                ss << "P(" << player_id << ") (" << variable_name << "," << p.second << " ) ";
            } else {
                if(variable_name.compare("final") == 0) break; //packet terminator
                server_info.m_keys[variable_name] = p.second;
                ss << "(" << variable_name << "," << p.second << ") ";
            }
            
            it++;
        }

		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", from_address.ToString().c_str(), ss.str().c_str());
		ss.str("");

        MM::MMPushRequest req;        

        req.from_address = from_address;
        req.v2_instance_key = 0;
        req.driver = this;
        req.server = server_info;
        req.version = 1;
        req.type = MM::EMMPushRequestType_Heartbeat;
        req.callback = on_v1_heartbeat_data_processed;
        AddRequest(req);
    }
}
    