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

		std::stringstream ss;
		
		//read unknown properties
		uint8_t num_addresses = buffer.ReadByte();
		ss << "Clients (";
		while(num_addresses--) {
			std::string ip_address = Read_FString(buffer);		
			ss << ip_address << " ";			
		}
		ss << ") ";
		buffer.ReadByte(); //??
		uint32_t unk2 = buffer.ReadInt(); 

		record.m_address.port = htons(buffer.ReadShort());
		ss << " Address: " << record.m_address.ToString();

		//read more unknown properties
		buffer.ReadByte(); buffer.ReadByte(); buffer.ReadByte();
		


		int hostname_len = buffer.ReadInt();
		record.hostname = buffer.ReadNTS();
		ss << " Hostname: " << record.hostname;

		record.level = Read_FString(buffer);
		ss << " Level: " << record.level;

		record.game_group = Read_FString(buffer);
		ss << " Game group: " << record.game_group;

		int num_players = buffer.ReadInt(), max_players = buffer.ReadInt(), unk5 = buffer.ReadInt(), unk6 = buffer.ReadInt();//, unk7 = buffer.ReadInt();
		record.num_players = num_players;
		record.max_players = max_players;
		ss << " Players: (" << record.num_players << "/" << record.max_players << ") ";
		
		uint8_t unk7 = buffer.ReadByte(), unk8 = buffer.ReadByte(), unk9 = buffer.ReadByte(), num_fields = buffer.ReadByte();

		int idx = num_fields;
		while(buffer.readRemaining() > 0) {
			std::string field = Read_FString(buffer);
			std::string property = Read_FString(buffer);

			ss << "(" << field << "," << property << "), ";


			if(stricmp(field.c_str(),"mutator") == 0) {
				record.m_mutators.push_back(property);
			} else {
				record.m_rules[field] = property;
			}
			if(--idx <= 0) break;
		} 
		int num_player_entries = buffer.ReadShort();

		ss << " Players (";
		idx = num_player_entries;
		while(buffer.readRemaining() > 0) {
			MM::PlayerRecord player_record;
			int name_len = buffer.ReadInt();
			

			player_record.name = buffer.ReadNTS();
			ss << player_record.name << ",";
			
			record.m_players.push_back(player_record);

			if(--idx <= 0) break;
		}
		ss << ")";
		OS::LogText(OS::ELogLevel_Info, "[%s] HB: %s", getAddress().ToString().c_str(), ss.str().c_str());

		m_server_address = record.m_address;

        TaskScheduler<MM::UTMasterRequest, TaskThreadData> *scheduler = ((UT::Server *)(this->GetDriver()->getServer()))->getScheduler();
        MM::UTMasterRequest req;        
        req.type = MM::UTMasterRequestType_Heartbeat;
		req.peer = this;
		req.peer->IncRef();
		req.callback = NULL;
		req.record = record;
        scheduler->AddRequest(req.type, req);

	}


    //this seems to be more stats related... but here for now, happens on non-stats games as well, just not much data
	void Peer::handle_newserver_request(OS::Buffer recv_buffer) {
		int unk1 = recv_buffer.ReadInt();
		if(recv_buffer.readRemaining() > 0) {
			
			int unk2 = recv_buffer.ReadInt();
			int unk3 = recv_buffer.ReadInt();
			bool continue_parse = true;
			std::string accumulated_string;
			while(continue_parse) {			
				while(true) {
					char b = recv_buffer.ReadByte();
					if(b == 0x00 || b == 0x09) {
						if(b == 0x0) {
							continue_parse = false;
							break;
						}
						accumulated_string = "";	
					} else {
						accumulated_string += b;
					}
				}
			}
			int unk4 = recv_buffer.ReadInt();
			int unk5 = recv_buffer.ReadInt();
			continue_parse = true;
			accumulated_string = "";
			while(continue_parse) {			
				while(true) {
					char b = recv_buffer.ReadByte();
					if(b == 0x00 || b == 0x09) {
						if(b == 0x0) {
							continue_parse = false;
							break;
						}
						accumulated_string = "";	
					} else {
						accumulated_string += b;
					}
				}
			}
			OS::LogText(OS::ELogLevel_Info, "[%s] Stats Init: %s", getAddress().ToString().c_str(), accumulated_string.c_str());
		}
		send_server_id(0); //init stats backend, generate match id, for now not needed
	}
	void Peer::send_server_id(int id) {
		m_state = EConnectionState_Heartbeat;
		OS::Buffer send_buffer;
		send_buffer.WriteByte(3);
		send_buffer.WriteInt(id);
		send_packet(send_buffer);
	}

}