#include <sstream>

#include "tasks.h"
#include <server/UTPeer.h>

namespace MM {
	int GetServerID(TaskThreadData *thread_data) {
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "INCR %s", mp_pk_name);
		Redis::Value v = resp.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}
		else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		}
		return ret;
	}
	bool isServerDeleted(TaskThreadData *thread_data, std::string server_key) {
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		int ret = -1;
		Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "HGET %s deleted", server_key.c_str());
		Redis::Value v = resp.values.front();
		 if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			ret = atoi(v.value._str.c_str());
		} else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			ret = v.value._int;
		}		
		return ret == 1;
	}
    bool serverRecordExists(TaskThreadData* thread_data, std::string server_key) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
        int ret = -1;
        Redis::Response resp = Redis::Command(thread_data->mp_redis_connection, 1, "EXISTS %s", server_key.c_str());
        Redis::Value v = resp.values.front();
        if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
            ret = atoi(v.value._str.c_str());
        }
        else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
            ret = v.value._int;
        }
        return ret == 1;
    }
    std::string GetServerKey_FromIPMap(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
        std::ostringstream s;
        s << "IPMAP_" << request.record.m_address.ToString(true) << "-" << request.record.m_address.GetPort();

        std::string server_key = "";
		Redis::Response reply;
		Redis::Value v;

        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "GET %s", s.str().c_str());
		if (Redis::CheckError(reply)) {
			return server_key;
		}
		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server_key = OS::strip_quotes(v.value._str).c_str();
		}

        if(server_key.length() != 0) {
            if(isServerDeleted(thread_data, server_key)) {
                return "";
            }
        }

        return server_key;
    }
    std::string GetServerKey_FromRequest(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info, bool &is_new_server, int &server_id) {

        is_new_server = false;
        std::string server_key = GetServerKey_FromIPMap(request, thread_data, game_info);
        if(server_key.length() > 0) {
            return server_key;
        }
        //lookip via IP map... else make new QR id and return server key
        server_id = GetServerID(thread_data);
        std::ostringstream s;
        s << game_info.gamename << ":" << server_id << ":";

        is_new_server = true;
        return s.str();
    }
    std::string GetMutatorsString(UTMasterRequest request) {
        std::ostringstream s;
        std::vector<std::string>::iterator it = request.record.m_mutators.begin();
        while(it != request.record.m_mutators.end()) {
            s << "\\" << (*it);
            it++;
        }
        return s.str();

    }
    void injectFilterVariables(UTMasterRequest request, TaskThreadData *thread_data, std::string cust_keys) {
        //for filters -- this stuff might end up being UT2004 specific
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s currentplayers %d", cust_keys.c_str(), request.record.num_players);        
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s freespace %d", cust_keys.c_str(), request.record.num_players < request.record.max_players);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s nomutators %s", cust_keys.c_str(), request.record.m_mutators.size() == 0 ? "true" : "false");
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s standard %s", cust_keys.c_str(), request.record.isStandardServer() ? "true" : "false");

        int nobots = true;
        if(request.record.m_rules.find("MinPlayers") != request.record.m_rules.end()) {
            std::string MinPlayersStr = request.record.m_rules["MinPlayers"];
            if(atoi(MinPlayersStr.c_str()) == 0) {
                nobots = true;
            } else {
                nobots = false;
            }
        }
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s nobots %s", cust_keys.c_str(), nobots ? "true" : "false");


        if(request.record.m_rules.find("Translocator") != request.record.m_rules.end()) {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s transloc \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.m_rules["Translocator"]).c_str());
        } else {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s transloc \"False\"", cust_keys.c_str());
        }

        if(request.record.m_rules.find("WeaponStay") != request.record.m_rules.end()) {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s weaponstay \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.m_rules["WeaponStay"]).c_str());
        } else {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s weaponstay \"False\"", cust_keys.c_str());
        }

        if(request.record.m_rules.find("GameStats") != request.record.m_rules.end()) {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s stats \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.m_rules["GameStats"]).c_str());
        } else {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s stats \"False\"", cust_keys.c_str());
        }

        if(request.record.m_rules.find("GamePassword") != request.record.m_rules.end()) {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s password \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.m_rules["GamePassword"]).c_str());
        } else {
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s password \"False\"", cust_keys.c_str());
        }
        
    }
    void WriteServerData(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info, std::string server_key, int id) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_port %d", server_key.c_str(), request.record.m_address.GetPort());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_ip \"%s\"", server_key.c_str(), request.record.m_address.ToString(true).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), game_info.gameid);
        if(id > 0)
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s id %d", server_key.c_str(), id);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s deleted 0", server_key.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HINCRBY %s num_updates 1", server_key.c_str());

        if(request.record.m_mutators.size() > 0) {
            std::string mutators = GetMutatorsString(request);
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s mutators \"%s\"", server_key.c_str(), OS::escapeJSON(mutators).c_str());
        }

        std::string ipinput = request.record.m_address.ToString(true);
        Redis::Command(thread_data->mp_redis_connection, 0, "SET IPMAP_%s-%d %s", ipinput.c_str(), request.record.m_address.GetPort(), server_key.c_str());

        //write cust keys
        std::string cust_keys = server_key + "custkeys";
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gamename \"%s\"", cust_keys.c_str(), game_info.gamename.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s hostname \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.hostname).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s mapname \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.level).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gametype \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.game_group).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s botlevel \"%s\"", cust_keys.c_str(), request.record.bot_level.c_str());

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s numplayers %d", cust_keys.c_str(), request.record.num_players);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s maxplayers %d", cust_keys.c_str(), request.record.max_players);

        injectFilterVariables(request, thread_data, cust_keys);

        std::map<std::string, std::string>::iterator it = request.record.m_rules.begin();
        while(it != request.record.m_rules.end()) {
            std::pair<std::string, std::string> p = *it;
            Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s \"%s\" \"%s\"", cust_keys.c_str(), OS::escapeJSON(p.first).c_str(), OS::escapeJSON(p.second).c_str());
            it++;
        }

        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s last_heartbeat %d", server_key.c_str(), current_time.tv_sec);

        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE IPMAP_%s-%d %d", request.record.m_address.ToString(true).c_str(), request.record.m_address.GetPort(), MM_REDIS_EXPIRE_TIME);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %s %d", server_key.c_str(), MM_REDIS_EXPIRE_TIME);
        Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %scustkeys %d", server_key.c_str(), MM_REDIS_EXPIRE_TIME);

        Redis::Command(thread_data->mp_redis_connection, 0, "ZADD %s %d \"%s\"", game_info.gamename.c_str(), id, server_key.c_str());
    }
    bool PerformHeartbeat(UTMasterRequest request, TaskThreadData *thread_data) {

        MMTaskResponse response;
        response.peer = request.peer;
        
        OS::GameData game_info = OS::GetGameByID(request.peer->GetGameId(), thread_data->mp_redis_connection);
        bool is_new_server = false;
        int server_id = 0;
        std::string key = GetServerKey_FromRequest(request, thread_data, game_info, is_new_server, server_id);

        WriteServerData(request, thread_data, game_info, key, server_id);

		std::ostringstream s;
        if(is_new_server) {
            //send AMQP new server event
			s << "\\new\\" << key.c_str();			
        } else {
            //send update
            s << "\\update\\" << key.c_str();
        }
        thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());

        if(request.peer) {
            request.peer->DecRef();
        }
        if(request.callback) {
            request.callback(response);
        }

        return true;
    }
}