#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {
	UsermodeRecord GetUsermodeFromJson(json_t* item) {
		UsermodeRecord record;

		json_t* subitem;

		record.isGlobal = true;

		subitem = json_object_get(item, "id");
		if (subitem != NULL) {
			record.usermodeid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "channelmask");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.chanmask = json_string_value(subitem);
		}

		subitem = json_object_get(item, "hostmask");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.hostmask = json_string_value(subitem);
		}

		subitem = json_object_get(item, "comment");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.comment = json_string_value(subitem);
		}

		subitem = json_object_get(item, "machineid");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.machineid = json_string_value(subitem);
		}

		subitem = json_object_get(item, "profileid");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.profileid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "modeflags");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.modeflags = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "expiresAt");
		record.expires_at.tv_sec = 0;
		record.expires_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.expires_at.tv_sec = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "ircNick");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.nick = json_string_value(subitem);
		}

		subitem = json_object_get(item, "setByHost");
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.address = OS::Address(json_string_value(subitem));
		}

		subitem = json_object_get(item, "setByPid");
		record.setByUserSummary.profileid = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.setByUserSummary.profileid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "setAt");
		record.set_at.tv_sec = 0;
		record.set_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.set_at.tv_sec = json_integer_value(subitem);
		}

		
		return record;
	}
	bool Perform_ListUsermodes(PeerchatBackendRequest request, TaskThreadData* thread_data) {

		json_t* send_json = json_object();
		json_object_set_new(send_json, "channelmask", json_string(request.usermodeRecord.chanmask.c_str()));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode/lookup";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		TaskResponse response;
		response.channel_summary.channel_name = request.usermodeRecord.chanmask;

		if (TaskShared::Handle_WebError(send_json, response.error_details)) {
			if (request.callback)
				request.callback(response, request.peer);
		}
		else {
			success = true;
			int num_items = json_array_size(send_json);
			for (int i = 0; i < num_items; i++) {
				json_t* item = json_array_get(send_json, i);

				response.is_start = i == 0;
				response.is_end = i == num_items - 1;

				UsermodeRecord record = GetUsermodeFromJson(item);
				response.usermode = record;

				if (request.callback)
					request.callback(response, request.peer);
			}
		}

		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}