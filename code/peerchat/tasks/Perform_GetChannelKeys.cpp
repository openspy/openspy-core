#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	const char *chankey_inject_keys[] = {"topic", "limit", "key"};

	bool Perform_GetChannelKeys_HandleOverrides(ChannelSummary summary, std::string name, std::string& output) {
		bool result = true;
		std::ostringstream ss;
		if(name.compare("topic") == 0) {
			ss << summary.topic;			
		}
		else if(name.compare("limit") == 0) {
			ss << summary.limit;
		}
		else if(name.compare("key") == 0) {
			ss << ((summary.password.length() > 0) ? 1 : 0);
		} else {
			result = false;
		}
		if(result) {
			output = ss.str();
		}
		
		return result;
	}
	void Perform_GetChannelKeys_InjectOverrides(ChannelSummary summary, std::string search_string, std::ostringstream& output) {
		for(size_t i=0;i<(sizeof(chankey_inject_keys)/sizeof(const char *));i++) {
			if(match(search_string.c_str(), chankey_inject_keys[i]) == 0) {
				output << "\\" << chankey_inject_keys[i] << "\\";
				std::string s;
				Perform_GetChannelKeys_HandleOverrides(summary, chankey_inject_keys[i], s);
				output << s;
			}
		}
	}
	bool Is_Chankey_InjectKey(const char *key) {
		for(size_t i=0;i<(sizeof(chankey_inject_keys)/sizeof(const char *));i++) {
			if(strcmp(chankey_inject_keys[i], key) == 0) return true;
		}
		return false;
	}
	void Handle_GetChannelKeys_LookupWildcard(TaskThreadData* thread_data, ChannelSummary summary, std::string search_string, std::ostringstream &ss) {
        Redis::Response reply;
		Redis::Value v, arr;
        int cursor = 0;

		Redis::SelectDb(thread_data->mp_redis_connection, OS::ERedisDB_Chat);
        do {
            reply = Redis::Command(thread_data->mp_redis_connection, 0, "HSCAN channel_%d %d MATCH custkey_%s", summary.channel_id, cursor, search_string.c_str());
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

		Perform_GetChannelKeys_InjectOverrides(summary, search_string, ss);
		error_cleanup:
		return;
	}

	bool Perform_GetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		response.channel_summary = summary;
		response.profile.uniquenick = request.profile.uniquenick; //getckey numeric value

		std::ostringstream ss;
		std::ostringstream wildcard_ss;

		if (summary.channel_id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
			std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
			while (it != iterators.second) {
				std::pair<std::string, std::string> p = *(it++);

				Redis::Response reply;
				Redis::Value v;

				if(p.first.find_first_of('*') == std::string::npos) {
					ss << "\\" << p.first << "\\";

					std::string computedOutput;

					if(!Perform_GetChannelKeys_HandleOverrides(summary, p.first, computedOutput)) {
						reply = Redis::Command(thread_data->mp_redis_connection, 0, "HGET channel_%d custkey_%s", summary.channel_id, p.first.c_str());
						if (reply.values.size() == 0 || reply.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR) {
							continue;
						}
						v = reply.values[0];

						
						if (v.type == Redis::REDIS_RESPONSE_TYPE_STRING) {
							ss << v.value._str;
						}
					} else {
						ss << computedOutput;
					}
				} else {
					Handle_GetChannelKeys_LookupWildcard(thread_data, summary, p.first, wildcard_ss);
				}
			}
		}

		response.kv_data = ss.str();
		response.kv_data_withnames = wildcard_ss.str();

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}