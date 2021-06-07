#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {
    ChanpropsRecord GetChanpropsFromJson(json_t* item) {
        ChanpropsRecord record;
		json_t* subitem = json_object_get(item, "id");
		if (subitem != NULL) {
			record.id = json_integer_value(subitem);
		}

        subitem = json_object_get(item, "channelmask");
		if (subitem != NULL) {
			record.channel_mask = json_string_value(subitem);
		}

        subitem = json_object_get(item, "password");
		if (subitem != NULL) {
			record.password = json_string_value(subitem);
		}

        subitem = json_object_get(item, "entrymsg");
		if (subitem != NULL) {
			record.entrymsg = json_string_value(subitem);
		}

        subitem = json_object_get(item, "comment");
		if (subitem != NULL) {
			record.comment = json_string_value(subitem);
		}

        subitem = json_object_get(item, "groupname");
		if (subitem != NULL) {
			record.groupname = json_string_value(subitem);
		}

		subitem = json_object_get(item, "limit");
		if (subitem != NULL) {
			record.limit = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "modeflags");
		if (subitem != NULL) {
			record.modeflags = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "onlyOwner");
		if (subitem != NULL) {
			record.modeflags = json_boolean_value(subitem);
		}

        subitem = json_object_get(item, "topic");
		if (subitem != NULL) {
			record.topic = json_string_value(subitem);
		}

        subitem = json_object_get(item, "setByNick");
		if (subitem != NULL) {
			record.setByNick = json_string_value(subitem);
		}
        subitem = json_object_get(item, "setByHost");
		if (subitem != NULL) {
			record.setByHost = json_string_value(subitem);
		}
        subitem = json_object_get(item, "setByPid");
		if (subitem != NULL) {
			record.setByPid = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "expiresAt");
		record.expires_at.tv_sec = 0;
		record.expires_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.expires_at.tv_sec = json_integer_value(subitem);
		}

		subitem = json_object_get(item, "setAt");
		record.set_at.tv_sec = 0;
		record.set_at.tv_usec = 0;
		if (subitem != NULL && !json_is_null(subitem)) {
			record.set_at.tv_sec = json_integer_value(subitem);
		}

        return record;
    }
    bool ApplyChanProps(ChannelSummary &summary) {
		json_t* send_json = json_object();
		json_object_set_new(send_json, "channelmask", json_string(summary.channel_name.c_str()));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Chanprops/ApplyEffectiveChanprops";

		OS::HTTPClient client(url);

		char* json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, NULL);

		free(json_data);
		json_decref(send_json);

		bool success = false;

		TaskShared::WebErrorDetails error_info;

		if (!TaskShared::Handle_WebError(send_json, error_info)) {
			success = true;
		}
		return success;
    }
}