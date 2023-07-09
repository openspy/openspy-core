#include <sstream>

#include "tasks.h"
#include <server/UTPeer.h>

namespace MM {
	int GetServerID(TaskThreadData *thread_data) {
		int ret = -1;
		redisReply *resp = (redisReply *) redisCommand(thread_data->mp_redis_connection, "INCR %s", mp_pk_name);
		if (resp == NULL) {
			return 0;
		}

        if (resp->type == REDIS_REPLY_INTEGER) {
            ret = resp->integer;
        }
        freeReplyObject(resp);
		return ret;
	}
	bool isServerDeleted(TaskThreadData *thread_data, std::string server_key) {
		int ret = -1;
		redisReply *resp = (redisReply *) redisCommand(thread_data->mp_redis_connection, "HGET %s deleted", server_key.c_str());
		if (resp == NULL) {
			return true;
		}

        if (resp->type == REDIS_REPLY_INTEGER) {
            ret = resp->integer;
        }
        freeReplyObject(resp);
		return ret == 1;
	}
    bool serverRecordExists(TaskThreadData* thread_data, std::string server_key) {
        int ret = -1;
        redisReply *resp = (redisReply *) redisCommand(thread_data->mp_redis_connection, "EXISTS %s", server_key.c_str());

		if (resp == NULL) {
			return false;
		}
        
        if (resp->type == REDIS_REPLY_INTEGER) {
            ret = resp->integer;
        }
        freeReplyObject(resp);
        return ret == 1;
    }
    std::string GetServerKey_FromIPMap(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info) {
        std::ostringstream s;
        s << "IPMAP_" << request.record.m_address.ToString(true) << "-" << request.record.m_address.GetPort();

        std::string ipmap_key = s.str();
        std::string server_key = "";


		redisReply *reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "GET %s", ipmap_key.c_str());
		if (reply == NULL) {
			return "";
		}
		if (reply->type == REDIS_REPLY_STRING) {
			server_key = reply->str;
		}
        freeReplyObject(reply);
        
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
        
        int nobots = true;
        if(request.record.m_rules.find("MinPlayers") != request.record.m_rules.end()) {
            std::string MinPlayersStr = request.record.m_rules["MinPlayers"];
            if(atoi(MinPlayersStr.c_str()) == 0) {
                nobots = true;
            } else {
                nobots = false;
            }
        }
        
        std::string transloc = "False";
        std::string weaponstay = "False";
        std::string stats = "False";
        std::string password = "False";
        
        std::ostringstream ss;
        
        std::string currentplayers;
        ss << request.record.num_players;
        currentplayers = ss.str();
        ss.str("");
        
        std::string freespace;
        ss << (request.record.num_players < request.record.max_players);
        freespace = ss.str();
        ss.str("");
        
        
        if(request.record.m_rules.find("Translocator") != request.record.m_rules.end()) {
            transloc = request.record.m_rules["Translocator"];
        }
        
        if(request.record.m_rules.find("WeaponStay") != request.record.m_rules.end()) {
            weaponstay = request.record.m_rules["WeaponStay"];
        }
        
        if(request.record.m_rules.find("GameStats") != request.record.m_rules.end()) {
            stats = request.record.m_rules["GameStats"];
        }
        
        if(request.record.m_rules.find("GamePassword") != request.record.m_rules.end()) {
            password = request.record.m_rules["GamePassword"];
        }
        
        const char *keys_cmd[] = {
            "HMSET", cust_keys.c_str(),
            "currentplayers", currentplayers.c_str(),
            "category", request.record.bot_level.c_str(),
            "freespace", freespace.c_str(),
            "nomutators", request.record.m_mutators.size() == 0 ? "true" : "false",
            "standard", request.record.isStandardServer() ? "true" : "false",
            "nobots", nobots ? "true" : "false",
            "transloc", transloc.c_str(),
            "weaponstay", weaponstay.c_str(),
            "stats", stats.c_str(),
            "password", password.c_str()
        };
        
        redisAppendCommandArgv(thread_data->mp_redis_connection, sizeof(keys_cmd) / sizeof(const char *), keys_cmd, NULL);
    }
    void WriteServerData(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info, std::string server_key, int id) {
        int num_cmds = 0;
        std::ostringstream ss;
        
        std::string mutators = GetMutatorsString(request);
        
        std::string wanport_str, wanip_str, gameid_str;
        
        ss << request.record.m_address.GetPort();
        wanport_str = ss.str();
        ss.str("");
        
        ss << request.record.m_address.ToString(true);
        wanip_str = ss.str();
        ss.str("");
        
        ss << request.record.gameid;
        gameid_str = ss.str();
        ss.str("");
        
        std::string last_heartbeat;
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        
        ss << current_time.tv_sec;
        last_heartbeat = ss.str();
        ss.str("");
        
        const char *basic_keys_cmd[] = {
            "HMSET", server_key.c_str(),
            "wan_port", wanport_str.c_str(),
            "wan_ip", wanip_str.c_str(),
            "gameid", gameid_str.c_str(),
            "id", server_key.c_str(),
            "deleted", "0",
            "mutators", mutators.c_str(),
            "last_heartbeat", last_heartbeat.c_str()
        };
        redisAppendCommandArgv(thread_data->mp_redis_connection, sizeof(basic_keys_cmd) / sizeof(const char *), basic_keys_cmd, NULL); num_cmds++;
        
        std::string cust_keys = server_key + "custkeys";
        
        std::string gamename = game_info.gamename;
        std::string numplayers_str, maxplayers_str;
        

        ss << request.record.num_players;
        numplayers_str = ss.str();
        ss.str("");
        
        ss << request.record.max_players;
        maxplayers_str = ss.str();
        ss.str("");
        
        
        const char *custkeys_cmd[] = {
            "HMSET", cust_keys.c_str(),
            "gamename", gamename.c_str(),
            "hostname", request.record.hostname.c_str(),
            "mapname", request.record.level.c_str(),
            "gametype", request.record.game_group.c_str(),
            "botlevel", request.record.bot_level.c_str(),
            "numplayers", numplayers_str.c_str(),
            "maxplayers", maxplayers_str.c_str()
        };
        redisAppendCommandArgv(thread_data->mp_redis_connection, sizeof(custkeys_cmd) / sizeof(const char *), custkeys_cmd, NULL); num_cmds++;
        
        //inject rules
        int num_keys = 2 + (request.record.m_rules.size() * 2);
        const char **rules_cmd = (const char **)malloc(sizeof(const char *) * num_keys);
        rules_cmd[0] = "HMSET";
        rules_cmd[1] = cust_keys.c_str();
        int idx = 2;
        std::map<std::string, std::string>::iterator it = request.record.m_rules.begin();
        while(it != request.record.m_rules.end()) {
            std::pair<std::string, std::string> p = *it;
            rules_cmd[idx++] = strdup(p.first.c_str());
            rules_cmd[idx++] = strdup(p.second.c_str());
            it++;
        }
        
        redisAppendCommandArgv(thread_data->mp_redis_connection, idx, rules_cmd, NULL); num_cmds++;
        for(int i = 2;i < idx; i++) {
            free((void *)rules_cmd[i]);
        }
        free((void *)rules_cmd);
        
        //inject filter stuff
        injectFilterVariables(request, thread_data, cust_keys); num_cmds++;
        
        
        ss << "IPMAP_" << request.record.m_address.ToString(true) << "-" << request.record.m_address.GetPort();
        
        std::string ipmap_str = ss.str();
        ss.str("");
        
        redisAppendCommand(thread_data->mp_redis_connection, "SET %s %s", ipmap_str.c_str(), server_key.c_str());  num_cmds++;
        
        redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", ipmap_str.c_str(), MM_REDIS_EXPIRE_TIME);  num_cmds++;
        
        redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", server_key.c_str(), MM_REDIS_EXPIRE_TIME);  num_cmds++;
        
        redisAppendCommand(thread_data->mp_redis_connection, "EXPIRE %s %d", cust_keys.c_str(), MM_REDIS_EXPIRE_TIME);  num_cmds++;
        
        redisAppendCommand(thread_data->mp_redis_connection, "ZADD %s %d %s", game_info.gamename.c_str(), id, server_key.c_str()); num_cmds++;
        
        for(int i=0;i<num_cmds;i++) {
            void *reply;
            int r = redisGetReply(thread_data->mp_redis_connection, (void **)&reply);
            if(r == REDIS_OK) {
                freeReplyObject(reply);
            }
        }
    }
    bool PerformHeartbeat(UTMasterRequest request, TaskThreadData *thread_data) {

        MMTaskResponse response;
        response.peer = request.peer;
        
        selectQRRedisDB(thread_data);
        
        OS::GameData game_info = request.peer->GetGameData();
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
