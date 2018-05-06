#include "FESLBackend.h"

namespace FESLBackend {
    OS::CThread *mp_async_thread;
    Redis::Connection *mp_redis_async_connection;
    const char *auth_channel = "auth.events";
    FESL::Server *mp_fesl_server;
	void *setup_redis_async(OS::CThread *thread) {
		struct timeval t;
		t.tv_usec = 0;
		t.tv_sec = 60;
		mp_redis_async_connection = Redis::Connect(OS::g_redisAddress, t);

		Redis::LoopingCommand(mp_redis_async_connection, 60, FESLBackend::onRedisMessage, thread->getParams(), "SUBSCRIBE %s", auth_channel);
		return NULL;
	}
    void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata) {
		Redis::Value v = reply.values.front();

		char msg_type[16], server_key[64];
		if (v.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {
			if (v.arr_value.values.size() == 3 && v.arr_value.values[2].first == Redis::REDIS_RESPONSE_TYPE_STRING) {
				if (strcmp(v.arr_value.values[1].second.value._str.c_str(), auth_channel) == 0) {
					std::map<std::string, std::string> map = OS::KeyStringToMap(v.arr_value.values[2].second.value._str);
					OS::Address remote_addr(map["remote_addr"].c_str());
					if (map["app_name"].compare("FESL") == 0) {
						mp_fesl_server->OnUserAuth(remote_addr, atoi(map["userid"].c_str()), atoi(map["profileid"].c_str()));
					}
				}
			}
		}
    }

    void SetupFESLBackend(FESL::Server *server) {
        mp_fesl_server = server;
		mp_async_thread = OS::CreateThread(setup_redis_async, NULL, true);
		OS::Sleep(200);

    }
    void ShutdownFESLBackend() {
		Redis::CancelLooping(mp_redis_async_connection);
		mp_async_thread->SignalExit(true);
		delete mp_async_thread;

		Redis::Disconnect(mp_redis_async_connection);
    }
}