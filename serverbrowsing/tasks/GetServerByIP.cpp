#include <tasks/tasks.h>
#include <tasks/tasks.h>
#include <server/SBPeer.h>
#include <sstream>
namespace MM {
    bool PerformGetServerByIP(MMQueryRequest request, TaskThreadData *thread_data) {
		ServerListQuery ret;
		Server *serv = GetServerByIP(thread_data, request.address, request.req.m_for_game);
		if(serv)
			ret.list.push_back(serv);

		if(request.peer) {
			request.peer->OnRetrievedServerInfo(request, ret, request.extra);
			request.peer->DecRef();
		}
		

		FreeServerListQuery(&ret);
        return true;
    }
	Server *GetServerByIP(TaskThreadData *thread_data, OS::Address address, OS::GameData game) {
		Server *server = NULL;

		std::ostringstream s;
		Redis::Response reply;
		Redis::Value v;
		
		Redis::Command(thread_data->mp_redis_connection, 0, "SELECT %d", OS::ERedisDB_QR);

		s << "GET IPMAP_" << address.ToString(true) << "-" << address.GetPort();
		std::string cmd = s.str();
		reply = Redis::Command(thread_data->mp_redis_connection, 0, cmd.c_str());
		if (reply.values.size() < 1 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			goto error_cleanup;

		v = reply.values.front();

		if(v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server = GetServerByKey(thread_data, v.value._str);
		}

		return server;

		error_cleanup:
			if (server)
				delete server;
			return NULL;
	}
}