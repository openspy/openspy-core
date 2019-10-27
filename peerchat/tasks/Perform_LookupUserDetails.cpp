#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {

	bool Perform_LookupUserDetailsByName(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;

		UserSummary summary = GetUserSummaryByName(thread_data, request.summary.username);
        response.summary = summary;
        

		if (summary.id != 0) {
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