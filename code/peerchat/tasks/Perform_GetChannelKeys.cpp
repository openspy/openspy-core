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
        redisReply *reply;
        int cursor = 0;

		std::string channel_key;
		std::ostringstream chan_ss;
		chan_ss << "channel_" << summary.channel_id << "_custkeys";
		channel_key = chan_ss.str();
        
        do {
            reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HSCAN %s %d MATCH *", channel_key.c_str(), cursor);
			
            // if (reply == NULL || thread_data->mp_redis_connection->err) {
            //     goto error_cleanup;
            // }
			// if (reply->elements < 2) {
			// 	goto error_cleanup;
			// }

			if(reply == NULL) {
				return;
			}

			if(thread_data->mp_redis_connection->err || reply->elements < 2) {
				freeReplyObject(reply);
				return;
			}

		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			std::string key;
			for(size_t i = 0; i< reply->element[1]->elements;i += 2) {
				if((i % 2) == 0) {
					key = reply->element[1]->element[i]->str;
				} else {
					ss << "\\" << key << "\\" << reply->element[1]->element[i]->str;
				}
			}
			freeReplyObject(reply);
        } while(cursor != 0);

		Perform_GetChannelKeys_InjectOverrides(summary, search_string, ss);
	}

	bool Perform_GetChannelKeys(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		ChannelSummary summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		response.channel_summary = summary;
		response.profile.uniquenick = request.profile.uniquenick; //getckey numeric value

		std::ostringstream ss;
		std::ostringstream wildcard_ss;
        
        ss << "channel_" << summary.channel_id << "_custkeys";
        std::string chan_key = ss.str();
        ss.str("");

		if (summary.channel_id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
			std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
			while (it != iterators.second) {
				std::pair<std::string, std::string> p = *(it++);

				redisReply* reply;

				if(p.first.find_first_of('*') == std::string::npos) {
					ss << "\\" << p.first << "\\";

					std::string computedOutput;

					if(!Perform_GetChannelKeys_HandleOverrides(summary, p.first, computedOutput)) {
						reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s %s", chan_key.c_str(), p.first.c_str());
						if (reply == NULL || thread_data->mp_redis_connection->err) {
							if(reply) {
								freeReplyObject(reply);
							}
							continue;
						}

						
						if (reply->type == REDIS_REPLY_STRING) {
							ss << reply->str;
						}
						freeReplyObject(reply);
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
