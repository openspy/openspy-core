#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {
	ChannelSummary ChanPropsToChannelSummary(ChanpropsRecord record) {
		ChannelSummary summary;
		summary.channel_name = record.channel_mask;
		summary.topic = record.topic;
		summary.limit = record.limit;
		summary.basic_mode_flags = record.modeflags;
		summary.password = record.password;
		return summary;
	}
	void AppendNonExistentPersistentChannels(PeerchatBackendRequest request, TaskThreadData *thread_data, std::vector<ChannelSummary> &channels) {
		//fetch all where modemask 64
		//if not exists in channels, append

		json_t* send_json = json_object();
		json_object_set_new(send_json, "modeflagMask", json_integer(EChannelMode_StayOpen));
		json_object_set_new(send_json, "exists", json_false());

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Chanprops/lookup";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		TaskResponse response;

		if (!TaskShared::Handle_WebError(send_json, response.error_details)) {
			int num_items = json_array_size(send_json);
			for (int i = 0; i < num_items; i++) {
				json_t* item = json_array_get(send_json, i);
				ChanpropsRecord record = GetChanpropsFromJson(item);
				if (request.channel_summary.channel_name.compare(record.groupname) == 0 || match2(request.channel_summary.channel_name.c_str(), record.channel_mask.c_str()) == 0) {
					channels.push_back(ChanPropsToChannelSummary(record));
				}
				
			}
		}
	}
	std::vector<ChannelSummary> ListChannels(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        
        int cursor = 0;
		std::vector<ChannelSummary> channels;
		ChannelSummary summary;

		redisReply *scan_reply, *reply;

		reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SELECT %d", OS::ERedisDB_Chat);
		freeReplyObject(reply);

        do {
            scan_reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "SCAN %d MATCH channelname_*", cursor);
			if (scan_reply == NULL)
				goto error_cleanup;

			if(thread_data->mp_redis_connection->err || scan_reply->elements < 2) {
				freeReplyObject(scan_reply);
				goto error_cleanup;
			}

		 	if(scan_reply->element[0]->type == REDIS_REPLY_STRING) {
		 		cursor = atoi(scan_reply->element[0]->str);
		 	} else if (scan_reply->element[0]->type == REDIS_REPLY_INTEGER) {
		 		cursor = scan_reply->element[0]->integer;
		 	}

			for (size_t i = 0; i < scan_reply->element[1]->elements; i++) {
				if (scan_reply->element[1]->element[i]->type != REDIS_REPLY_STRING)
					continue;

				std::string key = scan_reply->element[1]->element[i]->str;
				if (key.length() <= 12)
					continue;

				key = key.substr(12);
				summary = GetChannelSummaryByName(thread_data, key, false);

				std::ostringstream chan_ss;
				chan_ss << "channel_" << summary.channel_id;
				std::string channel_key = chan_ss.str();

				if (summary.channel_id != 0) {
					reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HGET %s custkey_groupname", channel_key.c_str());
					if (reply == NULL) {
						continue;
					}

					if(reply->type == REDIS_REPLY_STRING) {
						std::string group = reply->str;
						if (request.channel_summary.channel_name.compare(group) == 0 || match2(request.channel_summary.channel_name.c_str(), summary.channel_name.c_str()) == 0) {
							channels.push_back(summary);
						}
					}
					freeReplyObject(reply);
					
				}
			}
        } while(cursor != 0 && (channels.size() < (size_t)request.channel_summary.limit || request.channel_summary.limit == 0));

	AppendNonExistentPersistentChannels(request, thread_data, channels);
		
	error_cleanup:
		return channels;
    }

    bool Perform_ListChannels(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		TaskResponse response;
		response.summary.id = request.channel_summary.channel_id; //used for "specialinfo" (/list k)

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