#include <server/GSPeer.h>
#include "tasks.h"

namespace GS {
	bool Perform_GetGameInfo(PersistBackendRequest request, TaskThreadData *thread_data) {
		OS::GameData game_info;
		PersistBackendResponse resp_data;
        game_info = OS::GetGameByName(request.game_instance_identifier.c_str(), thread_data->mp_redis_connection);
		resp_data.gameData = game_info;
		callback_dispatch_later(game_info.secretkey[0] != 0, resp_data, request.mp_peer, request.mp_extra, request.callback);
		return true;
	}
}
