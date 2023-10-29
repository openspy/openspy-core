#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <OS/HTTP.h>
namespace Peerchat {

	int Create_StagingRoom_OwnerRecord(ChannelSummary channel, PeerchatBackendRequest request, TaskThreadData* thread_data) { 
		TaskResponse response;

		UsermodeRecord record;

		std::string formatted_name;
		std::transform(channel.channel_name.begin(),channel.channel_name.end(),std::back_inserter(formatted_name),tolower);
        
		record.chanmask = formatted_name;
		record.setByUserSummary = request.peer->GetUserDetails();
		record.comment = "Staging room - Creator record";
		record.hostmask = record.setByUserSummary.hostname;
		record.modeflags = EUserChannelFlag_Op;
		record.isGlobal = false;

		json_t* send_json = UsermodeRecordToJson(record);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Put(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		if (!TaskShared::Handle_WebError(send_json, response.error_details)) {			
			UsermodeRecord record = GetUsermodeFromJson(send_json);
			AssociateUsermodeToChannel(record, channel, thread_data);
		}

		if(send_json)
			json_decref(send_json);

		return record.modeflags;
	}
	int Create_StagingRoom_GameidRecord(ChannelSummary channel, PeerchatBackendRequest request, TaskThreadData* thread_data) { 
		TaskResponse response;

		UsermodeRecord record;
        
		std::string formatted_name;
		std::transform(channel.channel_name.begin(),channel.channel_name.end(),std::back_inserter(formatted_name),tolower);
        
		record.chanmask = formatted_name;
		record.setByUserSummary = request.peer->GetUserDetails();
		record.comment = "Staging room - Gameid Record";
		record.modeflags = EUserChannelFlag_GameidPermitted;
		record.gameid = record.setByUserSummary.gameid;
		record.isGlobal = false;

		json_t* send_json = UsermodeRecordToJson(record);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Put(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		if (!TaskShared::Handle_WebError(send_json, response.error_details)) {
			UsermodeRecord record = GetUsermodeFromJson(send_json);
			AssociateUsermodeToChannel(record, channel, thread_data);
		}

		if(send_json)
			json_decref(send_json);

		return record.modeflags;
	}
	int Create_StagingRoom_UsermodeRecords(ChannelSummary channel, PeerchatBackendRequest request, TaskThreadData* thread_data) {
		int flags = 0;
		flags |= Create_StagingRoom_OwnerRecord(channel, request, thread_data);
		flags |= Create_StagingRoom_GameidRecord(channel, request, thread_data); 
		return flags;
	}
	void DeleteTemporaryUsermodesForChannel(TaskThreadData* thread_data, ChannelSummary channel) {
		int channel_id = channel.channel_id;
		int cursor = 0;
		redisReply *scan_reply, *reply;

		std::ostringstream ss;
		ss << "channel_" << channel_id << "_usermodes";
		std::string channel_usermodes_key = ss.str();

		//scan channel usermodes
		do {
			scan_reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "HSCAN %s %d match *", channel_usermodes_key.c_str(), cursor);
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

			for(size_t i=0;i<scan_reply->element[1]->elements;i++) {
				if(i % 2 == 0) {
					int usermode_id = atoi(scan_reply->element[1]->str);
					if(usermode_id < 0) {
						std::string keyname = std::string("USERMODE_") + scan_reply->element[1]->str;
						reply = (redisReply *)redisCommand(thread_data->mp_redis_connection, "DEL %s", keyname.c_str());
						freeReplyObject(reply);
					}
				}
			}
            freeReplyObject(scan_reply);
		} while(cursor != 0);
	}
	bool Perform_SetGlobalUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) { 
		TaskResponse response;
        
		request.usermodeRecord.setByUserSummary = request.peer->GetUserDetails();
		json_t* send_json = UsermodeRecordToJson(request.usermodeRecord);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Put(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		if (!TaskShared::Handle_WebError(send_json, response.error_details)) {
			UsermodeRecord record = GetUsermodeFromJson(send_json);
			response.usermode = record;
		}

		if(send_json)
			json_decref(send_json);
		
		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}


	
	bool Perform_SetUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		return Perform_SetGlobalUsermode(request, thread_data);
	}
}
