#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
    int Peer::get_server_flags(MM::ServerRecord record) {
		if(m_client_version <= 3000) {
			return get_server_flags_ut2003(record);
		}
        int flags = 0;
        if(record.m_rules.find("GamePassword") != record.m_rules.end()) {
            if(record.m_rules["GamePassword"].compare("True") == 0) {
                flags |= (1 << 0);
            }
        }

        if(record.m_rules.find("GameStats") != record.m_rules.end()) {
            if(record.m_rules["GameStats"].compare("True") == 0) {
                flags |= (1 << 1);
            }
        }

        if(record.m_rules.find("ServerVersion") != record.m_rules.end()) {
            std::string version = record.m_rules["ServerVersion"];
            if(atoi(version.c_str()) == m_config->latest_client_version) {
                flags |= (1 << 2);
            }
        }

        if(record.m_rules.find("ServerMode") != record.m_rules.end()) {
            if(record.m_rules["ServerMode"].compare("non-dedicated") == 0) {
                flags |= (1 << 3);
            }
        }
        
		if(record.isStandardServer()) {
			flags |= (1 << 5);
		}
		
        std::vector<std::string>::iterator it = record.m_mutators.begin();
        while(it != record.m_mutators.end()) {
            std::string mutator = *it;
			if(mutator.compare("MutUTClassic") == 0) {
				flags |= (1 << 6);
			} else if(mutator.compare("MutInstaGib") == 0) {
				flags |= (1 << 4);
			}
            it++;
        }
        return flags;
    }
    int Peer::get_server_flags_ut2003(MM::ServerRecord record) {
        int flags = 0;
        if(record.m_rules.find("GamePassword") != record.m_rules.end()) {
            if(record.m_rules["GamePassword"].compare("True") == 0) {
                flags |= (1 << 6);
            }
        }

        if(record.m_rules.find("GameStats") != record.m_rules.end()) {
            if(record.m_rules["GameStats"].compare("True") == 0) {
                flags |= (1 << 5);
            }
        }

        if(record.m_rules.find("ServerVersion") != record.m_rules.end()) {
            std::string version = record.m_rules["ServerVersion"];
            if(atoi(version.c_str()) == m_config->latest_client_version) {
                flags |= (1 << 4);
            }
        }

        if(record.m_rules.find("ServerMode") != record.m_rules.end()) {
            if(record.m_rules["ServerMode"].compare("non-dedicated") == 0) {
                flags |= (1 << 3);
            }
        }
        
        std::vector<std::string>::iterator it = record.m_mutators.begin();
        while(it != record.m_mutators.end()) {
            std::string mutator = *it;
			if(mutator.compare("MutInstaGib") == 0) {
				flags |= (1 << 2);
			}
            it++;
        }
        return flags;
    }
	void Peer::on_get_server_list(MM::MMTaskResponse response) {
		OS::Buffer send_buffer;
		send_buffer.WriteInt(response.server_records.size());
		send_buffer.WriteByte(1);

		response.peer->send_packet(send_buffer);
		
		std::vector<MM::ServerRecord>::iterator it = response.server_records.begin();
		while(it != response.server_records.end()) {
			MM::ServerRecord server = *it;

			send_buffer.resetCursors();
			send_buffer.WriteInt(server.m_address.ip);
			send_buffer.WriteShort(server.m_address.port); //game port
			send_buffer.WriteShort(server.m_address.port + 1); //query port (maybe this can be something other than +1?)

			Write_FString(server.hostname, send_buffer);
			Write_FString(server.level, send_buffer);
			Write_FString(server.game_group, send_buffer);

			send_buffer.WriteByte(server.num_players);
			send_buffer.WriteByte(server.max_players);
			if(response.peer->m_client_version >= 3000) {
				send_buffer.WriteInt(response.peer->get_server_flags(server)); //flags
				Write_FString("1", send_buffer);
			} else {
				send_buffer.WriteByte(response.peer->get_server_flags(server)); //flags
			}
			response.peer->send_packet(send_buffer);
			it++;
		}
		response.peer->Delete();
	}
	void Peer::handle_request_server_list(OS::Buffer recv_buffer) {
		TaskScheduler<MM::UTMasterRequest, TaskThreadData> *scheduler = ((UT::Server *)(this->GetDriver()->getServer()))->getScheduler();
        MM::UTMasterRequest req;     

		std::stringstream ss;

		char num_filter_fields = recv_buffer.ReadByte();

		for(int i=0;i<num_filter_fields;i++) {
			std::string field = Read_FString(recv_buffer);

			std::string property = Read_FString(recv_buffer);
			

			int is_negate = recv_buffer.ReadByte();		

			MM::FilterProperties filter;
			filter.field = field;
			filter.property = property;
			filter.is_negate = is_negate;

			if(is_negate) {
				ss << field << "!="  << property << ",";
			} else {
				ss << field << "="  << property << ",";
			}
			req.m_filters.push_back(filter);
		}

		OS::LogText(OS::ELogLevel_Info, "[%s] Server list request: %s", getAddress().ToString().c_str(), ss.str().c_str());

           
        req.type = MM::UTMasterRequestType_ListServers;
		req.peer = this;
		req.peer->IncRef();
		req.callback = on_get_server_list;
        scheduler->AddRequest(req.type, req);

	}
}