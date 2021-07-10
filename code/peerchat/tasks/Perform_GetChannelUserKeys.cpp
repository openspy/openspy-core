#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	void Handle_GetChannelUserKeys_LookupWildcard(TaskThreadData* thread_data, int channel_id, int user_id, std::string search_string, std::ostringstream &ss) {

        Redis::Response reply;
		Redis::Value v, arr;
        int cursor = 0;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
        do {
            reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN channel_%d_user_%d %d MATCH custkey_%s", channel_id, user_id, cursor, search_string.c_str());
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
				std::string key;
				for (size_t i = 0; i < arr.arr_value.values.size(); i++) {
					if (arr.arr_value.values[i].first != Redis::REDIS_RESPONSE_TYPE_STRING)
						continue;

					
					
					if((i % 2) == 0) {
						key = arr.arr_value.values[i].second.value._str;
						key = key.substr(8); //skip custkey_
					} else {
						ss << "\\" << key << "\\" << arr.arr_value.values[i].second.value._str;
					}
					
				}
			}else break;
        } while(cursor != 0);
		error_cleanup:
		return;
	}

	void Handle_GetChannelUserKeys_SingleUser(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		UserSummary user_summary = GetUserSummaryByName(thread_data, request.summary.username);
		response.channel_summary = summary;
		response.summary = user_summary;
		response.profile.uniquenick = request.profile.uniquenick; //getckey numeric value

		std::ostringstream ss;
		std::ostringstream wildcard_ss;

		if (summary.channel_id != 0 && user_summary.id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
			std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
			while (it != iterators.second) {
				std::pair<std::string, std::string> p = *(it++);

				Redis::Response reply;
				Redis::Value v;

				if(p.first.find_first_of('*') == std::string::npos) {
					reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d_user_%d custkey_%s", summary.channel_id, user_summary.id, p.first.c_str());
					if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
						continue;
					}
					v = reply.values[0];

					ss << "\\" << p.first << "\\";
					if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
						ss << v.value._str;
					}
				} else {
					Handle_GetChannelUserKeys_LookupWildcard(thread_data, summary.channel_id, user_summary.id, p.first, wildcard_ss);
				}
			}
		}

		response.kv_data = ss.str();
		response.kv_data_withnames = wildcard_ss.str();
		response.profile.id = 1; //indicate end

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
	}
	void Handle_GetChannelUserKeys_AllUsers(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		
		response.channel_summary = summary;

		response.profile.uniquenick = request.profile.uniquenick; //getckey numeric value

		std::ostringstream ss;
		std::ostringstream wildcard_ss;

		if (summary.channel_id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;

			std::vector<ChannelUserSummary> users = GetChannelUsers(thread_data, summary.channel_id);
			std::vector<ChannelUserSummary>::iterator it = users.begin();
			while (it != users.end()) {
				ChannelUserSummary user = *(it++);

				response.summary = user.userSummary;
				response.channelUserSummary = user;


				std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
				std::vector<std::pair< std::string, std::string> >::const_iterator it2 = iterators.first;
				while (it2 != iterators.second) {
					std::pair<std::string, std::string> p = *(it2++);

					Redis::Response reply;
					Redis::Value v;
					if(p.first.find_first_of('*') == std::string::npos) {
						reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d_user_%d custkey_%s", summary.channel_id, user.user_id, p.first.c_str());
						if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
							continue;
						}
						v = reply.values[0];

						ss << "\\" << p.first << "\\";
						if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
							ss << v.value._str;
						}
					} else {
						Handle_GetChannelUserKeys_LookupWildcard(thread_data, summary.channel_id, user.user_id, p.first, wildcard_ss);
					}
					
				}

				
				if (it == users.end()) {
					response.profile.id = 1; //indicate end
				}
				else {
					response.profile.id = 0;
				}

				response.kv_data = ss.str();
				response.kv_data_withnames = wildcard_ss.str();
				ss.str("");
				wildcard_ss.str("");
				if (request.callback) {
					request.callback(response, request.peer);
				}
				response.kv_data = OS::KVReader();
				response.kv_data_withnames = OS::KVReader();
			}			
		}

		if (request.peer) {
			request.peer->DecRef();
		}
	}

    bool Perform_GetChannelUserKeys(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		if (request.summary.username.compare("*") == 0) { //scan all users
			Handle_GetChannelUserKeys_AllUsers(request, thread_data);
		}
		else {
			Handle_GetChannelUserKeys_SingleUser(request, thread_data);
		}
		
        return true;
    }
}