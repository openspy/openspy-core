#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <OS/HTTP.h>

namespace Peerchat {
    bool SetOrRemoveNonGlobalBan_ByChannel(PeerchatBackendRequest request, std::string channel, std::string banmask, TaskShared::WebErrorDetails &error_details, bool set) {
        TaskResponse response;

        UsermodeRecord usermode;
        usermode.setByUserSummary = request.peer->GetUserDetails();

        usermode.comment = "Issued via MODE command";
        usermode.hostmask = banmask;
        usermode.chanmask = channel;
        usermode.isGlobal = false;
        usermode.modeflags = EUserChannelFlag_Banned;
        usermode.has_gameid = false;
            
        
        json_t* send_json = UsermodeRecordToJson(usermode);
        

        std::string url = std::string(OS::g_webServicesURL) + "/v1/Usermode";

        OS::HTTPClient client(url);

        char* json_data = json_dumps(send_json, 0);

        OS::HTTPResponse resp;
        if(set) {
            resp = client.Put(json_data, request.peer);
        } else {
            resp = client.Delete(json_data, request.peer);
        }

		free(json_data);
		json_decref(send_json);

        send_json = json_loads(resp.buffer.c_str(), 0, NULL);

        bool success = true;
        if (!TaskShared::Handle_WebError(send_json, error_details)) {
            success = false;
        }

		if(send_json)
			json_decref(send_json);

        return success;
    }

    std::string GetFormattedHostMask(std::string mask) {
        size_t idx = mask.find_last_of('@');
        if(idx != std::string::npos && mask.length() > idx) {
            return mask.substr(idx+1);
        }
        return mask;
    }
    bool Perform_UpdateChannelModes_BanMask(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        TaskResponse response;
        bool success = true;
        std::map<std::string, int>::iterator it = request.channel_modify.set_usermodes.begin();
        while(it != request.channel_modify.set_usermodes.end()) {
            std::pair<std::string, int> p = *(it++);
            if(p.second & EUserChannelFlag_Banned) {
                if(!SetOrRemoveNonGlobalBan_ByChannel(request, request.channel_summary.channel_name, GetFormattedHostMask(p.first), response.error_details, true)) {
                    break;
                }
            }                
        }

        it = request.channel_modify.unset_usermodes.begin();
        while(it != request.channel_modify.unset_usermodes.end()) {
            std::pair<std::string, int> p = *(it++);
            if(p.second & EUserChannelFlag_Banned) {
                if(!SetOrRemoveNonGlobalBan_ByChannel(request, request.channel_summary.channel_name, GetFormattedHostMask(p.first), response.error_details, false)) {
                    break;
                }
            }                
        }

        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}