#include <tasks/tasks.h>
#include <server/QRPeer.h>
#include <sstream>
namespace MM {
    bool PerformPushServer(MMPushRequest request, TaskThreadData  *thread_data) {
		int pk_id = PushServer(thread_data, request.server, true, request.server.id);
		bool success = false;
		if (request.server.id != pk_id && request.peer) {
			request.peer->OnRegisteredServer(pk_id);
			success = true;
		}
		if(request.peer) {
			request.peer->DecRef();
		}
		return success;
		
    }
	int PushServer(TaskThreadData *thread_data, ServerInfo server, bool publish, int pk_id) {
		int id = pk_id;
		int groupid = 0;

		if (id == -1) {
			id = TryFindServerID(thread_data, server);
			if (id == -1) {
				id = GetServerID(thread_data);
			}
		}

		server.id = id;
		server.groupid = groupid;

		std::ostringstream s;
		s << server.m_game.gamename << ":" << groupid << ":" << id << ":";
		std::string server_key = s.str();

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);

		if(pk_id == -1) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), server.m_game.gameid);
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s id %d", server_key.c_str(), id);

			Redis::Command(thread_data->mp_redis_connection, 0, "HDEL %s deleted", server_key.c_str()); //incase resume
		}

		
		std::string ipinput = server.m_address.ToString(true);



		Redis::Command(thread_data->mp_redis_connection, 0, "SET IPMAP_%s-%d %s", ipinput.c_str(), server.m_address.GetPort(), server_key.c_str());
		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE IPMAP_%s-%d %d", ipinput.c_str(), server.m_address.GetPort(), MM_PUSH_EXPIRE_TIME);


		if(pk_id == -1) {
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s gameid %d", server_key.c_str(), server.m_game.gameid);
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_port %d", server_key.c_str(), server.m_address.GetPort());
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %s wan_ip \"%s\"", server_key.c_str(), ipinput.c_str());
		}
		else {
			Redis::Command(thread_data->mp_redis_connection, 0, "ZINCRBY %s 1 \"%s\"", server.m_game.gamename.c_str(), server_key.c_str());
		}

		Redis::Command(thread_data->mp_redis_connection, 0, "HINCRBY %s num_beats 1", server_key.c_str());


		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %s %d", server_key.c_str(), MM_PUSH_EXPIRE_TIME);

		std::map<std::string, std::string>::iterator it = server.m_keys.begin();
		while (it != server.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			Redis::Command(thread_data->mp_redis_connection, 0, "HSET %scustkeys %s \"%s\"", server_key.c_str(), p.first.c_str(), OS::escapeJSON(p.second).c_str());
			it++;
		}
		Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %scustkeys %d", server_key.c_str(), MM_PUSH_EXPIRE_TIME);

		std::map<std::string, std::vector<std::string> >::iterator it2 = server.m_player_keys.begin();

		int i = 0, max_idx = 0;
		std::pair<std::string, std::vector<std::string> > p;
		std::vector<std::string>::iterator it3;
		while (it2 != server.m_player_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {
				std::string s = *it3;
				if(s.length() > 0) {
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %scustkeys_player_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
				}
				i++;
				it3++;
			}
			max_idx= i;
			i = 0;
			it2++;
		}

		for(i=0;i<max_idx;i++) {
			Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %scustkeys_player_%d %d", server_key.c_str(), i, MM_PUSH_EXPIRE_TIME);
		}
		i=0;


		it2 = server.m_team_keys.begin();
		while (it2 != server.m_team_keys.end()) {
			p = *it2;
			it3 = p.second.begin();
			while (it3 != p.second.end()) {

				std::string s = *it3;
				if(s.length() > 0) {
					Redis::Command(thread_data->mp_redis_connection, 0, "HSET %scustkeys_team_%d %s \"%s\"", server_key.c_str(), i, p.first.c_str(), OS::escapeJSON(s).c_str());
					i++;
				}
				it3++;
			}
			max_idx= i;
			i = 0;
			it2++;
		}
		for(i=0;i<max_idx;i++) {
			Redis::Command(thread_data->mp_redis_connection, 0, "EXPIRE %scustkeys_team_%d %d", server_key.c_str(), i, MM_PUSH_EXPIRE_TIME);
		}
		i=0;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_QR);
		if (publish) {
			Redis::Command(thread_data->mp_redis_connection, 0, "ZADD %s %d \"%s\"", server.m_game.gamename.c_str(), pk_id, server_key.c_str());

			std::ostringstream s;
			s << "\\new\\" << server_key.c_str();
			thread_data->mp_mqconnection->sendMessage(mm_channel_exchange, mm_server_event_routingkey, s.str());
		}

		return id;

	}
}