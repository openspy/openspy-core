#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	std::vector<ChannelSummary> ListChannels(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        Redis::Response reply;
		Redis::Value v, arr;
        int cursor = 0;
		std::vector<ChannelSummary> channels;
		ChannelSummary summary;
		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
        do {
            reply = Redis::Command(thread_data->mp_redis_connection, 0, "SCAN %d MATCH channelname_*", cursor);
			if (Redis::CheckError(reply))
				goto error_cleanup;

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

					std::string key = arr.arr_value.values[i].second.value._str;
					if (key.length() <= 12)
						continue;

					key = key.substr(12);
					summary = GetChannelSummaryByName(thread_data, key, false);
					if (summary.channel_id != 0)
						channels.push_back(summary);
				}
			}
			else break;

        } while(cursor != 0);
	error_cleanup:
		return channels;
    }

    bool Perform_ListChannels(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		TaskResponse response;

        response.channel_summaries = ListChannels(request, thread_data);
		
		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}