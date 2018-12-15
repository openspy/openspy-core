#include <sstream>
#include <algorithm>
#include <tasks/tasks.h>
#include <server/QRPeer.h>
namespace MM {
    bool PerformUpdateServer(MMPushRequest request, TaskThreadData  *thread_data) {
		//find missing keys and push deletes
		std::map<std::string, std::string>::iterator it = request.old_server.m_keys.begin();
		std::map<std::string, std::vector<std::string> >::iterator it2;
		std::vector<std::string>::iterator it3;
		std::vector<std::string> missing_keys, missing_team_keys, missing_player_keys;

		Redis::Value v;
		Redis::Response resp;
		std::string name;
		size_t idx = 0;
		bool force_delete = false;
		bool change_occured = false;
		ServerInfo modified_server;

		modified_server.groupid = 0;

		while (it != request.old_server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			if(request.server.m_keys.find(p.first) == request.server.m_keys.end()) {
				missing_keys.push_back(p.first);
			}
			it++;
		}		

		it2 = request.old_server.m_player_keys.begin();
		while(it2 != request.old_server.m_player_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(request.server.m_player_keys.find(p.first) == request.server.m_player_keys.end()) {
				missing_player_keys.push_back(p.first);
			}
			it2++;
		}
		
		it2 = request.old_server.m_team_keys.begin();
		while(it2 != request.old_server.m_team_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(request.server.m_team_keys.find(p.first) == request.server.m_team_keys.end()) {
				missing_team_keys.push_back(p.first);
			}
			it2++;
		}

		
		std::ostringstream ss;
		ss << request.server.m_game.gamename << ":" << request.server.groupid << ":" << request.server.id << ":";
		std::string server_key = ss.str();



		Redis::Command(thread_data->mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		it3 = missing_keys.begin();
		while(it3 != missing_keys.end()) {
			std::string s = *it3;
			Redis::Command(thread_data->mp_redis_connection, 0, "HDEL %scustkeys %s", ss.str().c_str(),s.c_str());
			it3++;
		}

		ss.str("");
		while(true) {
			force_delete = false;
			if(request.server.m_player_keys.size() > 0) {
				if(idx > (*request.server.m_player_keys.begin()).second.size()) {
					force_delete = true;
				}
			}
			ss.str("");
			ss << server_key << "custkeys_player_" << idx++;
			resp = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS %s", ss.str().c_str());
			v = resp.values.front();
			int ret = -1;
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				ret = v.value._int;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				ret = atoi(v.value._str.c_str());
			}
			if(ret <= 0) {
				break;
			} else if(force_delete) {
				Redis::Command(thread_data->mp_redis_connection, 0, "DEL %s", ss.str().c_str());
				break;
			}
			it3 = missing_player_keys.begin();
			while(it3 != missing_player_keys.end()) {
				name = *it3;
				Redis::Command(thread_data->mp_redis_connection, 0, "HDEL %s %s", ss.str().c_str(), name.c_str());
				it3++;
			}
		}
		
		idx = 0;
		ss.str("");
		while(true) {
			force_delete = false;
			if(request.server.m_team_keys.size() > 0) {
				if(idx > (*request.server.m_team_keys.begin()).second.size()) {
					force_delete = true;
				}
			}
			ss.str("");
			ss << server_key << "custkeys_team_" << idx++;
			resp = Redis::Command(thread_data->mp_redis_connection, 0, "EXISTS %s", ss.str().c_str());
			v = resp.values.front();
			int ret = -1;
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
				ret = v.value._int;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
				ret = atoi(v.value._str.c_str());
			}
			if(ret <= 0) {
				break;
			} else if(force_delete) {
				Redis::Command(thread_data->mp_redis_connection, 0, "DEL %s", ss.str().c_str());
				break;
			}
			it3 = missing_team_keys.begin();
			while(it3 != missing_team_keys.end()) {
				std::string name = *it3;
				Redis::Command(thread_data->mp_redis_connection, 0, "HDEL %s %s", ss.str().c_str(), name.c_str());
				it3++;
			}
		}

		//create ServerInfo struct with changed keys only
		modified_server.m_game = request.server.m_game;
		modified_server.id = request.server.id;
		modified_server.m_address = request.server.m_address;
		it = request.server.m_keys.begin();
		while(it != request.server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			if(std::find(missing_keys.begin(), missing_keys.end(), p.first) == missing_keys.end()) {
				if(request.server.m_keys[p.first].compare(request.old_server.m_keys[p.first]) != 0) {
					modified_server.m_keys[p.first] = request.server.m_keys[p.first];
					change_occured = true;
				} 
			}
			it++;
		}

		it2 = request.server.m_player_keys.begin();
		while(it2 != request.server.m_player_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(std::find(missing_player_keys.begin(), missing_player_keys.end(), p.first) == missing_player_keys.end()) {
				it3 = p.second.begin();
				idx = 0;
				while(it3 != p.second.end()) {
					if(request.old_server.m_player_keys[p.first].size() > idx && request.server.m_player_keys[p.first].size() > idx && request.server.m_player_keys[p.first][idx].compare(request.old_server.m_player_keys[p.first][idx]) == 0) {
						modified_server.m_player_keys[p.first].push_back(std::string());
					} else {
						modified_server.m_player_keys[p.first].push_back(request.server.m_player_keys[p.first][idx]);
						change_occured = true;
					}
					idx++;
					it3++;
				}
			}
			it2++;
		}

		it2 = request.server.m_team_keys.begin();
		while(it2 != request.server.m_team_keys.end()) {
			std::pair<std::string, std::vector<std::string> > p = *it2;
			if(std::find(missing_team_keys.begin(), missing_team_keys.end(), p.first) == missing_team_keys.end()) {
				it3 = p.second.begin();
				idx = 0;
				while(it3 != p.second.end()) {
					if(request.old_server.m_team_keys.size() > idx && request.server.m_team_keys.size() > idx && request.server.m_team_keys[p.first][idx].compare(request.old_server.m_team_keys[p.first][idx]) == 0) {
						modified_server.m_team_keys[p.first].push_back(std::string());
					} else {
						modified_server.m_team_keys[p.first].push_back(request.server.m_team_keys[p.first][idx]);
						change_occured = true;
					}
					idx++;
					it3++;
				}
			}
			it2++;
		}
		PushServer(thread_data, modified_server, false, request.server.id); //push to prevent expire

		if(change_occured) {
			std::ostringstream s;
			s << "\\update\\" << modified_server.m_game.gamename << ":" << modified_server.groupid << ":" << modified_server.id << ":";
			thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey,s.str());
		}
		if(request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}