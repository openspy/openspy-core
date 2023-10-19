#include <tasks/tasks.h>
#include <server/SBPeer.h>
namespace MM {
    bool PerformGetGameInfoByGameName(MMQueryRequest request, TaskThreadData *thread_data) {
		OS::GameData game_info;
        game_info = OS::GetGameByName(request.gamenames[0].c_str(), thread_data->mp_redis_connection);
        if(request.peer) {
            request.peer->OnRecievedGameInfo(game_info, request.extra);
            request.peer->DecRef();
        }
        return true;
    }
}