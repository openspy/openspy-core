#include <server/QRPeer.h>
#include <tasks/tasks.h>
#include <sstream>
namespace MM {
    bool PerformDeleteServer(MMPushRequest request, TaskThreadData  *thread_data) {
        DeleteServer(thread_data, request.server, true);
		if(request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
	void DeleteServer(TaskThreadData *thread_data, MM::ServerInfo server, bool publish) {
		int groupid = server.groupid;
		int id = server.id;

		std::ostringstream ss;
		ss << server.m_game.gamename << ":" << server.groupid << ":" << server.id;
		std::string entry_name = ss.str();


		Redis::Command(thread_data->mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);
		Redis::Command(thread_data->mp_redis_connection, 0, "ZREM %s \"%s:%d:%d:\"", server.m_game.gamename.c_str(), server.m_game.gamename.c_str(), server.groupid, server.id);
		if (publish) {
			Redis::Response reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s deleted", entry_name.c_str());
			Redis::Value v = reply.values[0];
			if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER && v.value._int == 1) {
				return;
			}
			else if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING && v.value._str.compare("1") == 0) {
				return;
			}
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s:%d:%d: deleted 1", server.m_game.gamename.c_str(), server.groupid, server.id);
			Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %s:%d:%d: %d", server.m_game.gamename.c_str(), server.groupid, server.id, MM_PUSH_EXPIRE_TIME);

			std::ostringstream s;
			s << "\\del\\" << server.m_game.gamename.c_str() << ":" << groupid << ":" << id << ":";
			thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());
		}
		else {
			Redis::Command(thread_data->mp_redis_connection, 0, "DEL %s:%d:%d:", server.m_game.gamename.c_str(), server.groupid, server.id);
			Redis::Command(thread_data->mp_redis_connection, 0, "DEL %s:%d:%d:custkeys", server.m_game.gamename.c_str(), server.groupid, server.id);

			int i = 0;
			int groupid = server.groupid;
			int id = server.id;

			std::map<std::string, std::vector<std::string> >::iterator it2 = server.m_player_keys.begin();

			std::pair<std::string, std::vector<std::string> > p;
			std::vector<std::string>::iterator it3;
			while (it2 != server.m_player_keys.end()) {
				p = *it2;
				it3 = p.second.begin();
				while (it3 != p.second.end()) { //XXX: will be duplicate deletes but better than writing stuff to delete indivually atm, rewrite later though
					std::string s = *it3;
					Redis::Command(thread_data->mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_player_%d", server.m_game.gamename.c_str(), groupid, id, i);
					i++;
					it3++;
				}

				i = 0;
				it2++;
			}


			it2 = server.m_team_keys.begin();
			while (it2 != server.m_team_keys.end()) {
				p = *it2;
				it3 = p.second.begin();
				while (it3 != p.second.end()) { //XXX: will be duplicate deletes but better than writing stuff to delete indivually atm, rewrite later though
					std::string s = *it3;
					Redis::Command(thread_data->mp_redis_connection, 0, "DEL %s:%d:%d:custkeys_team_%d", server.m_game.gamename.c_str(), groupid, id, i);
					i++;
					it3++;
				}

				i = 0;
				it2++;
			}
		}
    }
}