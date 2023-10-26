#include "tasks.h"

namespace FESL {
	const char *auth_channel_exchange = "presence.core";
	const char *auth_routingkey = "presence.buddies";

	TaskShared::ListenerEventHandler consume_authevent_handler = {"openspy.core", "auth.events", Handle_AuthEvent};
	TaskShared::ListenerEventHandler all_events[] = {consume_authevent_handler};
	TaskShared::ListenerArgs fesl_event_handler = {all_events, sizeof(all_events) / sizeof(TaskShared::ListenerEventHandler)};

	bool Handle_AuthEvent(TaskThreadData *thread_data, std::string message) {
		FESL::Server *server = (FESL::Server *)uv_loop_get_data(uv_default_loop());
		OS::KVReader reader = OS::KVReader(message);
		std::string msg_type = reader.GetValue("type");
		if (msg_type.compare("auth_event") == 0) {
			std::string appName = reader.GetValue("app_name");
			if (appName.compare("FESL") == 0) {
				int profileid = reader.GetValueInt("profileid");
				int userid = reader.GetValueInt("userid");
				std::string session_key = reader.GetValue("session_key");
				server->OnUserAuth(session_key, userid, profileid);
			}
		}
		return true;
	}
	void InitTasks() {
		setup_listener(&fesl_event_handler);
	}
	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		temp_data.mp_redis_connection = TaskShared::getThreadLocalRedisContext();
		FESLRequest *work_data = (FESLRequest *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(work_data->type) {
			case EFESLRequestType_GetEntitledGameFeatures:
				Perform_GetEntitledGameFeatures(*work_data, &temp_data);
				break;
			case EFESLRequestType_GetObjectInventory:
				Perform_GetObjectInventory(*work_data, &temp_data);
				break;

		}
	}
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		FESLRequest *work_data = (FESLRequest *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);
	}
    void AddFESLTaskRequest(FESLRequest request) {
			uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));

			FESLRequest *work_data = new FESLRequest();
			*work_data = request;

			uv_handle_set_data((uv_handle_t*) uv_req, work_data);
			uv_queue_work(uv_default_loop(), uv_req, PerformUVWorkRequest, PerformUVWorkRequestCleanup);
	}
}