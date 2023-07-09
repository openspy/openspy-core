#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
    bool Perform_UpdateUserModes(PeerchatBackendRequest request, TaskThreadData *thread_data) {
		TaskResponse response;
		UserSummary summary = LookupUserById(thread_data, request.peer->GetBackendId());
		for (int i = 0; i < num_user_mode_flags; i++) {
			if (request.channel_modify.set_mode_flags & user_mode_flag_map[i].flag) {
				if (~summary.modeflags & user_mode_flag_map[i].flag) {
					summary.modeflags |= user_mode_flag_map[i].flag;
				}
			}
			if (request.channel_modify.unset_mode_flags & user_mode_flag_map[i].flag) {
				if (summary.modeflags & user_mode_flag_map[i].flag) {
					summary.modeflags &= ~user_mode_flag_map[i].flag;
				}
			}
		}

		void *reply;

		reply = redisCommand(thread_data->mp_redis_connection, "HSET user_%d modeflags %d", summary.id, summary.modeflags);

		freeReplyObject(reply);

		response.error_details.response_code = TaskShared::WebErrorCode_Success;

		response.summary = summary;

		if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}