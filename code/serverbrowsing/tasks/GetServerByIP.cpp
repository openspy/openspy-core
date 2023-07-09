#include <tasks/tasks.h>
#include <tasks/tasks.h>
#include <server/SBPeer.h>
#include <sstream>
namespace MM {
    bool PerformGetServerByIP(MMQueryRequest request, TaskThreadData *thread_data) {
		ServerListQuery ret;
		Server *serv = GetServerByIP(thread_data, request.address, request.req.m_for_game, request.req.include_deleted, request.req.all_player_keys && request.req.all_team_keys);
		if(serv)
			ret.list.push_back(serv);

		if(request.peer) {
			request.peer->OnRetrievedServerInfo(request, ret, request.extra);
			request.peer->DecRef();
		}
		

		FreeServerListQuery(&ret);
        return true;
    }
	Server *GetServerByIP(TaskThreadData *thread_data, OS::Address address, OS::GameData game, bool include_deleted, bool all_player_and_team_keys) {
		Server *server = NULL;

		std::ostringstream s;
		
		redisReply *reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_QR);
		freeReplyObject(reply);

		s << "GET IPMAP_" << address.ToString(true) << "-" << address.GetPort();
		std::string cmd = s.str();

		reply = (redisReply *) redisCommand(thread_data->mp_redis_connection, cmd.c_str());

		if(reply) {
			if(reply->type == REDIS_REPLY_STRING) {
				server = GetServerByKey(thread_data, reply->str, include_deleted, all_player_and_team_keys);
			}
			freeReplyObject(reply);
		}

		return server;
	}
}
