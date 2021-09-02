#include <sstream>

#include "tasks.h"
#include <server/UTPeer.h>

namespace MM {
    std::string GetServerKey_FromRequest(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info) {
        int server_id = request.peer->GetServerID();
        std::ostringstream s;
        s << game_info.gamename << ":" << server_id << ":";
        return s.str();
    }
    void WriteServerData(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info, std::string server_key, int id) {
        Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_port %d", server_key.c_str(), request.record.m_address.GetPort());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_ip \"%s\"", server_key.c_str(), request.record.m_address.ToString(true).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), game_info.gameid);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s id %d", server_key.c_str(), id);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s deleted 0", server_key.c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HINCRBY %s num_updates 1", server_key.c_str());

        std::string ipinput = request.record.m_address.ToString(true);
        Redis::Command(thread_data->mp_redis_connection, 0, "SET IPMAP_%s-%d %s", ipinput.c_str(), request.record.m_address.GetPort(), server_key.c_str());

        //write cust keys
        std::string cust_keys = server_key + "custkeys";
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s hostname \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.hostname).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s mapname \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.level).c_str());
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s game_group \"%s\"", cust_keys.c_str(), OS::escapeJSON(request.record.game_group).c_str());

        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s numplayers %d", cust_keys.c_str(), request.record.num_players);
        Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s maxplayers %d", cust_keys.c_str(), request.record.max_players);

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
        std::string key = GetServerKey_FromRequest(request, thread_data, game_info);

        WriteServerData(request, thread_data, game_info, key, request.peer->GetServerID());

        if(request.peer) {
            request.peer->DecRef();
        }
        if(request.callback) {
            request.callback(response);
        }

        return true;
    }
}