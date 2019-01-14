#include "tasks.h"
#include <server/GSPeer.h>
namespace GS {
        TaskScheduler<PersistBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
            TaskScheduler<PersistBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PersistBackendRequest, TaskThreadData>(4, server);
			scheduler->AddRequestHandler(EPersistRequestType_SetUserData, Perform_SetUserData);
			scheduler->AddRequestHandler(EPersistRequestType_GetUserData, Perform_GetUserData);
			scheduler->AddRequestHandler(EPersistRequestType_NewGame, Perform_NewGame);
			scheduler->AddRequestHandler(EPersistRequestType_UpdateGame, Perform_UpdateGame);
			scheduler->AddRequestHandler(EPersistRequestType_GetGameInfoByGamename, Perform_GetGameInfo);

			scheduler->DeclareReady();

            return scheduler;
        }
}