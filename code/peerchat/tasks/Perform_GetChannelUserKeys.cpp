#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	void Handle_GetChannelUserKeys_LookupWildcard(TaskThreadData* thread_data, int channel_id, int user_id, std::string search_string, std::ostringstream &ss) {

        redisReply *reply;
        int cursor = 0;

		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
		freeReplyObject(reply);
        do {
            reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HSCAN channel_%d_user_%d %d MATCH custkey_%s", channel_id, user_id, cursor, search_string.c_str());

			if(reply == NULL) {
				goto error_cleanup;
			}

			if(thread_data->mp_redis_connection->err || reply->elements < 2) {
				freeReplyObject(reply);
				goto error_cleanup;
			}

		 	if(reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(reply->element[0]->str);
		 	} else if (reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = reply->element[0]->integer;
		 	}

			std::string key;
			for (size_t i = 0; i < reply->element[1]->elements; i++) {
				if (reply->element[1]->element[i]->type != REDIS_REPLY_STRING)
					continue;
			
				
				if((i % 2) == 0) {
					key = reply->element[1]->element[i]->str;
					key = key.substr(8); //skip custkey_
				} else {
					ss << "\\" << key << "\\" << reply->element[1]->element[i]->str;
				}
				
			}
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

		redisReply *reply;

		if (summary.channel_id != 0 && user_summary.id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> iterators = request.channel_modify.kv_data.GetHead();
			std::vector<std::pair< std::string, std::string> >::const_iterator it = iterators.first;
			while (it != iterators.second) {
				std::pair<std::string, std::string> p = *(it++);

				if(p.first.find_first_of('*') == std::string::npos) {
					reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET channel_%d_user_%d custkey_%s", summary.channel_id, user_summary.id, p.first.c_str());
					if (reply == NULL) {
						continue;
					}

					ss << "\\" << p.first << "\\";
					if(reply->type == REDIS_REPLY_STRING) {
						ss << reply->str;
					}
					
					freeReplyObject(reply);
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

		redisReply *reply;

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

					if(p.first.find_first_of('*') == std::string::npos) {
						reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET channel_%d_user_%d custkey_%s", summary.channel_id, user.user_id, p.first.c_str());
						if (reply == NULL) {
							continue;
						}

						
						ss << "\\" << p.first << "\\";
						if (reply->type == REDIS_REPLY_STRING) {
							ss << reply->str;
						}
						freeReplyObject(reply);
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