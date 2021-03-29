#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	void AssignChannelSummariesToResponse(TaskThreadData* thread_data, TaskResponse &response, int user_id) {
		Redis::Response reply;

		std::string key;
		Redis::Value v, arr;

		int cursor = 0;
        int channel_id;

        do {
            reply = Redis::Command(thread_data->mp_redis_connection, 0, "SCAN %d MATCH channel_*_user_%d", cursor, user_id);
            if (Redis::CheckError(reply))
                return;

            v = reply.values[0].arr_value.values[0].second;
            arr = reply.values[0].arr_value.values[1].second;

			if (arr.type == Redis::REDIS_RESPONSE_TYPE_ARRAY) {

				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					cursor = atoi(v.value._str.c_str());
				}
				else if (v.type == Redis::REDIS_RESPONSE_TYPE_INTEGER) {
					cursor = v.value._int;
				}
				for (size_t i = 0; i < arr.arr_value.values.size(); i++) {
					if (arr.arr_value.values[i].first != Redis::REDIS_RESPONSE_TYPE_STRING)
						continue;

					key = arr.arr_value.values[i].second.value._str;
                    
                    reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET %s channel_id" , key.c_str());

                    channel_id = atoi(reply.values[0].value._str.c_str());

                    ChannelSummary channel_summary = LookupChannelById(thread_data, channel_id);

					response.channel_summaries.push_back(channel_summary);
                }
            }
        } while(cursor != 0);
	}
	bool Perform_LookupUserDetailsByName(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		UserSummary summary = GetUserSummaryByName(thread_data, request.summary.username);
        response.summary = summary;
		response.profile.uniquenick = request.summary.username;

		if (summary.id != 0) {
			AssignChannelSummariesToResponse(thread_data, response, summary.id);
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
		}
		else {
			response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
		}

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}