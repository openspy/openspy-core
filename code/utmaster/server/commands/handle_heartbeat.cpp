#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>
namespace UT {

	void Peer::handle_heartbeat(OS::Buffer buffer) {
		MM::ServerRecord record;
		record.m_address = getAddress();
		record.gameid = m_config->gameid;

		std::stringstream ss;
		
		//read addresses
		int num_addresses = Read_CompactInt(buffer);
		ss << "Clients (";
		while(num_addresses--) {
			std::string ip_address = Read_FString(buffer, true);
			ss << ip_address << " ";			
		}
		ss << ") ";

		ss << " Unk1: " << buffer.ReadInt(); //unknown data

		ss << " Address: " << Read_FString(buffer, true);

		record.m_address.port = htons(buffer.ReadInt());
		ss << " Port: " << record.m_address.ToString();
		ss << " Unk2: " << buffer.ReadInt();

		record.hostname = Read_FString(buffer, true);
		ss << " Hostname: " << record.hostname;

		record.level = Read_FString(buffer, true);
		ss << " Level: " << record.level;

		record.game_group = Read_FString(buffer, true);
		ss << " Game group: " << record.game_group;

		int num_players = buffer.ReadInt(), max_players = buffer.ReadInt(); /*, unk5 = buffer.ReadInt(); */ 
		ss << " Unk3: " << buffer.ReadInt();
		
		record.num_players = num_players;
		record.max_players = max_players;
		ss << " Players: (" << record.num_players << "/" << record.max_players << ") ";
		
		if(m_client_version >= 3000) {
			ss << " Unk4: " << buffer.ReadInt();

			record.bot_level = Read_FString(buffer, true);
			ss << " Bot: " << record.bot_level << " ";
		}
		
		int num_fields = Read_CompactInt(buffer);

		for(int i=0;i<num_fields;i++) {
			std::string field = Read_FString(buffer, true);
			std::string property = Read_FString(buffer, true);

			ss << "(" << field << "," << property << "), ";

			if(field.compare("mutator") == 0) {
				record.m_mutators.push_back(property);
			} else {
				record.m_rules[field] = property;
			}
		} 
		int num_player_entries = Read_CompactInt(buffer);
		
		ss << " Players (";
		for(int i=0;i<num_player_entries;i++) {
			MM::PlayerRecord player_record;
			player_record.id = buffer.ReadInt();
			if (m_client_version == 2226) { //skip UT2XMP specific data
				buffer.ReadInt();
			}
			player_record.name = Read_FString(buffer, true);

			player_record.ping = buffer.ReadInt();
			player_record.score = buffer.ReadInt();
			player_record.stats_id = buffer.ReadInt();

			ss << player_record.name << "(" << player_record.id << "," << player_record.ping << "," << player_record.score << "," << player_record.stats_id << "),";
			
			record.m_players.push_back(player_record);
		}
		ss << ")";
		OS::LogText(OS::ELogLevel_Info, "[%s] HB: %s", getAddress().ToString().c_str(), ss.str().c_str());

		m_server_address = record.m_address;

		//inject version for UT2003
		ss.str("");
		ss << m_client_version;
		record.m_rules["ServerVersion"] = ss.str();

        MM::UTMasterRequest req;        
        req.type = MM::UTMasterRequestType_Heartbeat;
		req.peer = this;
		req.peer->IncRef();
		req.callback = NULL;
		req.record = record;
        AddRequest(req);

	}

	void Peer::send_heartbeat_request(uint8_t id, uint32_t code) {
		OS::Buffer send_buffer;
		send_buffer.WriteByte(EServerOutgoingRequest_RequestHeartbeat);
		send_buffer.WriteByte(id);
		send_buffer.WriteInt(code);
		send_packet(send_buffer);
	}
	void Peer::send_server_gamestatsid(int id) {
		OS::Buffer send_buffer;
		send_buffer.WriteByte(3);
		send_buffer.WriteInt(id);
		send_packet(send_buffer);
	}

}
