#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

    bool Perform_LookupChannelDetails(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		TaskResponse response;
		ChannelSummary summary;
		if(request.channel_summary.channel_id != 0) {			
			summary = LookupChannelById(thread_data, request.channel_summary.channel_id);
		} else {
			summary = GetChannelSummaryByName(thread_data, request.channel_summary.channel_name, false);
		}
        
		if (summary.channel_id != 0) {
			response.error_details.response_code = TaskShared::WebErrorCode_Success;
		}
		else {
			response.error_details.response_code = TaskShared::WebErrorCode_NoSuchUser;
		}

		response.profile.uniquenick = request.channel_summary.channel_name;
		response.channel_summary = summary;
		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}