#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_GetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		response.channel_summary = summary;
		response.profile.uniquenick = request.profile.uniquenick; //getckey numeric value

		std::ostringstream ss;

		if (summary.channel_id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
			std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
			while (it != iterators.second) {
				std::pair<std::string, std::string> p = *(it++);

				Redis::Response reply;
				Redis::Value v;
				
				reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d custkey_%s", summary.channel_id, p.first.c_str());
				if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
					continue;
				}
				v = reply.values[0];

				ss << "\\" << p.first << "\\";
				if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
					ss << v.value._str;
				}
			}
		}

		response.kv_data = ss.str();

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}