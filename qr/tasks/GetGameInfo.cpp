#include <server/QRPeer.h>
#include <tasks/tasks.h>
namespace MM {
    bool PerformGetGameInfo(MMPushRequest request, TaskThreadData  *thread_data) {
		OS::GameData game_info = OS::GetGameByName(request.gamename.c_str(), thread_data->mp_redis_connection);
		request.peer->OnGetGameInfo(game_info, request.state);
        return true;
    }
}