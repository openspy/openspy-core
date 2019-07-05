#include <tasks/tasks.h>
#include <server/SBPeer.h>
namespace MM {
    bool PerformGetGameInfoByGameName(MMQueryRequest request, TaskThreadData *thread_data) {
		OS::GameData game_info;
        if(!thread_data->shared_game_cache->LookupGameByName(request.gamenames[0].c_str(), game_info)) {
            game_info = OS::GetGameByName(request.gamenames[0].c_str(), thread_data->mp_redis_connection);
            thread_data->shared_game_cache->AddGame(thread_data->id, game_info);
        }
        if(request.peer) {
            request.peer->OnRecievedGameInfo(game_info, request.extra);
            request.peer->DecRef();
        }
        return true;
    }
}