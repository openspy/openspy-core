#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>

#include "tasks.h"


namespace MM {
	const char *mm_channel_exchange = "openspy.master";

	const char *mm_client_message_routingkey = "client.message";
	const char *mm_server_event_routingkey = "server.event";

    const char *mp_pk_name = "QRID";

	TaskScheduler<MMQueryRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
		{EMMQueryRequestType_GetServers, PerformGetServers},
		{EMMQueryRequestType_GetGroups, PerformGetGroups},
		{EMMQueryRequestType_GetServerByKey, PerformGetServerByKey},
		{EMMQueryRequestType_GetServerByIP, PerformGetServerByIP},
		{EMMQueryRequestType_SubmitData, PerformSubmitData},
		{EMMQueryRequestType_GetGameInfoByGameName, PerformGetGameInfoByGameName},
		{EMMQueryRequestType_GetGameInfoPairByGameName, PerformGetGameInfoPairByGameName},
		{-1, NULL}
	};

	TaskScheduler<MMQueryRequest, TaskThreadData>::ListenerHandlerEntry listenerTable[] = {
		{mm_channel_exchange, mm_server_event_routingkey, Handle_ServerEventMsg},
		{NULL, NULL, NULL}
	};

    TaskScheduler<MMQueryRequest, TaskThreadData> *InitTasks(INetServer *server) {
        TaskScheduler<MMQueryRequest, TaskThreadData> *scheduler = new TaskScheduler<MMQueryRequest, TaskThreadData>(OS::g_numAsync, server, requestTable, listenerTable);

        scheduler->SetThreadDataFactory(TaskScheduler<MMQueryRequest, TaskThreadData>::DefaultThreadDataFactory);

		scheduler->DeclareReady();

        return scheduler;
    }

	bool FindAppend_PlayerKVFields(TaskThreadData *thread_data, Server *server, std::string entry_name, std::string key, int index)
	 {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return false;

		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server->kvPlayers[index][key] = (v.value._str);
		}
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			server->kvPlayers[index][key] = v.value._int;
		}

		return true;

	}
	bool FindAppend_TeamKVFields(TaskThreadData *thread_data, Server *server, std::string entry_name, std::string key, int index) {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return false;

		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server->kvTeams[index][key] = (v.value._str);
		}
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			server->kvTeams[index][key] = v.value._int;
		}

		return true;

	}
	bool FindAppend_ServKVFields(TaskThreadData *thread_data, Server *server, std::string entry_name, std::string key) {
		Redis::Response reply;
		Redis::Value v;
		reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s %s", entry_name.c_str(), key.c_str());
		if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR)
			return false;

		v = reply.values.front();
		if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
			server->kvFields[key] = (v.value._str);
		}
		if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
			server->kvFields[key] = v.value._int;
		}

		return true;

	}
	void FreeServerListQuery(MM::ServerListQuery *query) {
		std::vector<Server *>::iterator it = query->list.begin();
		while(it != query->list.end()) {
			Server *server = *it;
			delete server;
			it++;
		}
	}

}