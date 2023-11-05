#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	void AssignChannelSummariesToResponse(TaskThreadData* thread_data, TaskResponse &response, int user_id) {
		redisReply *scan_reply, *reply;

		int cursor = 0;
        int channel_id;

        do {
			std::ostringstream chan_ss;
			chan_ss << "channel_*_user_" << user_id;
			std::string chan_user_key = chan_ss.str();
            scan_reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SCAN %d MATCH %s", cursor, chan_user_key.c_str());
			if (scan_reply == NULL)
				return;


			if(thread_data->mp_redis_connection->err || scan_reply->elements < 2) {
				freeReplyObject(scan_reply);
				return;
			}

		 	if(scan_reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(scan_reply->element[0]->str);
		 	} else if (scan_reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = scan_reply->element[0]->integer;
		 	}

			for (size_t i = 0; i < scan_reply->element[1]->elements; i++) {

				std::string key = scan_reply->element[1]->element[i]->str;
				
				reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s channel_id" , key.c_str());

				channel_id = atoi(reply->str);

				freeReplyObject(reply);

				ChannelSummary channel_summary = LookupChannelById(thread_data, channel_id);

				response.channel_summaries.push_back(channel_summary);
			}
			freeReplyObject(scan_reply);
        } while(cursor != 0);
	}
	int CountUserChannels(TaskThreadData *thread_data, int user_id) {
		redisReply *scan_reply;

		int cursor = 0;
        int count = 0;

        do {
            scan_reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SCAN %d MATCH channel_*_user_%d", cursor, user_id);
            if (scan_reply == NULL)
                return 0;


		 	if(scan_reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(scan_reply->element[0]->str);
		 	} else if (scan_reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = scan_reply->element[0]->integer;
		 	}

			count += scan_reply->element[1]->elements;
			freeReplyObject(scan_reply);
        } while(cursor != 0);
		return count;
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
