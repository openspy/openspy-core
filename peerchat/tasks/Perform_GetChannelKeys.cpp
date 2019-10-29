#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	void Handle_GetChannelKeys_SingleUser(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		UserSummary user_summary = GetUserSummaryByName(thread_data, request.summary.username);
		response.channel_summary = summary;
		response.summary = user_summary;
		response.profile.uniquenick = request.profile.uniquenick; //getckey numeric value

		if (summary.channel_id != 0 && user_summary.id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			std::map<std::string, std::string>::iterator it = request.channel_modify.kv_data.begin();
			while (it != request.channel_modify.kv_data.end()) {
				std::pair<std::string, std::string> p = *(it++);

				Redis::Response reply;
				Redis::Value v;

				response.kv_data[p.first] = "";
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d_user_%d custkey_%s", summary.channel_id, user_summary.id, p.first.c_str());
				if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
					continue;
				}
				v = reply.values[0];

				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					response.kv_data[p.first] = v.value._str;
				}
			}
		}

		response.profile.id = 1; //indicate end

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
	}
	void Handle_GetChannelKeys_AllUsers(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		
		response.channel_summary = summary;

		response.profile.uniquenick = request.profile.uniquenick; //getckey numeric value

		if (summary.channel_id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;

			std::vector<ChannelUserSummary> users = GetChannelUsers(thread_data, summary.channel_id);
			std::vector<ChannelUserSummary>::iterator it = users.begin();
			while (it != users.end()) {
				ChannelUserSummary user = *(it++);

				response.summary = LookupUserById(thread_data, user.user_id);

				std::map<std::string, std::string>::iterator it2 = request.channel_modify.kv_data.begin();
				while (it2 != request.channel_modify.kv_data.end()) {
					std::pair<std::string, std::string> p = *(it2++);

					Redis::Response reply;
					Redis::Value v;

					response.kv_data[p.first] = "";
					reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d_user_%d custkey_%s", summary.channel_id, user.user_id, p.first.c_str());
					if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
						continue;
					}
					v = reply.values[0];

					if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						response.kv_data[p.first] = v.value._str;
					}
				}

				
				if (it == users.end()) {
					response.profile.id = 1; //indicate end
				}
				else {
					response.profile.id = 0;
				}
				if (request.callback) {
					request.callback(response, request.peer);
				}
				response.kv_data.clear();
			}			
		}

		if (request.peer) {
			request.peer->DecRef();
		}
	}

    bool Perform_GetChannelKeys(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		if (request.summary.username.compare("*") == 0) { //scan all users
			Handle_GetChannelKeys_AllUsers(request, thread_data);
		}
		else {
			Handle_GetChannelKeys_SingleUser(request, thread_data);
		}
		
        return true;
    }
}