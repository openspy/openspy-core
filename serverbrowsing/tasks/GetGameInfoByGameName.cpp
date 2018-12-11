#include <tasks/tasks.h>
#include <server/SBPeer.h>
namespace MM {
    bool PerformGetGameInfoByGameName(MMQueryRequest request, TaskThreadData *thread_data) {
		OS::GameData game = OS::GetGameByName(request.gamenames[0].c_str(), thread_data->mp_redis_connection);

		request.peer->OnRecievedGameInfo(game, request.extra);
        return true;
    }
}