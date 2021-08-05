#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {


    json_t *ChanPropsToJson(ChanpropsRecord record) {
        json_t *send_json = json_object();

		std::string formatted_name;
		std::transform(record.channel_mask.begin(),record.channel_mask.end(),std::back_inserter(formatted_name),tolower);

        json_object_set_new(send_json, "channelmask", json_string(formatted_name.c_str()));

        if(record.password.length() > 0)
            json_object_set_new(send_json, "password", json_string(record.password.c_str()));
        else 
            json_object_set_new(send_json, "password", json_null());

        if(record.entrymsg.length() > 0)
            json_object_set_new(send_json, "entrymsg", json_string(record.entrymsg.c_str()));
        else 
            json_object_set_new(send_json, "entrymsg", json_null());

        if(record.comment.length() > 0)
            json_object_set_new(send_json, "comment", json_string(record.comment.c_str()));
        else 
            json_object_set_new(send_json, "comment", json_null());

        if(record.groupname.length() > 0)
            json_object_set_new(send_json, "groupname", json_string(record.groupname.c_str()));
        else 
            json_object_set_new(send_json, "groupname", json_null());

        if(record.topic.length() > 0)
            json_object_set_new(send_json, "topic", json_string(record.topic.c_str()));
        else 
            json_object_set_new(send_json, "topic", json_null());

        json_object_set_new(send_json, "limit", json_integer(record.limit));
        json_object_set_new(send_json, "modeflags", json_integer(record.modeflags));

        json_object_set_new(send_json, "onlyOwner", record.onlyOwner ? json_true() : json_false());
        json_object_set_new(send_json, "kickExisting", record.kickExisting ? json_true() : json_false());

        json_object_set_new(send_json, "setByPid", json_integer(record.setByPid));
        json_object_set_new(send_json, "setByNick", json_string(record.setByNick.c_str()));
        json_object_set_new(send_json, "setByHost", json_string(record.setByHost.c_str()));


        return send_json;

    }
	bool Perform_SetChanprops(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		json_t* send_json = ChanPropsToJson(request.chanpropsRecord);

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Chanprops";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Put(json_data, request.peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		TaskResponse response;

		if (TaskShared::Handle_WebError(send_json, response.error_details)) {
			if (request.callback)
				request.callback(response, request.peer);
		}
		else {
			success = true;
			ChanpropsRecord record = GetChanpropsFromJson(send_json);
            response.chanprops = record;

            if (request.callback)
                request.callback(response, request.peer);
		}

		if(send_json)
			json_decref(send_json);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}

}