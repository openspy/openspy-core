#include "tasks.h"
#include <server/GSPeer.h>
namespace GS {
        TaskScheduler<PersistBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
            TaskScheduler<PersistBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PersistBackendRequest, TaskThreadData>(OS::g_numAsync, server);
			scheduler->AddRequestHandler(EPersistRequestType_SetUserData, Perform_SetUserData);
			scheduler->AddRequestHandler(EPersistRequestType_GetUserData, Perform_GetUserData);
			scheduler->AddRequestHandler(EPersistRequestType_GetUserKeyedData, Perform_GetUserKeyedData);
			scheduler->AddRequestHandler(EPersistRequestType_NewGame, Perform_NewGame);
			scheduler->AddRequestHandler(EPersistRequestType_UpdateGame, Perform_UpdateGame);
			scheduler->AddRequestHandler(EPersistRequestType_GetGameInfoByGamename, Perform_GetGameInfo);
			scheduler->AddRequestHandler(EPersistRequestType_Auth_ProfileID, Perform_ProfileIDAuth);
			scheduler->AddRequestHandler(EPersistRequestType_Auth_AuthTicket, Perform_AuthTokenAuth);
			scheduler->AddRequestHandler(EPersistRequestType_Auth_CDKey, Perform_CDKeyAuth);
			scheduler->AddRequestHandler(EPersistRequestType_GetProfileIDFromCDKey, Perform_GetProfileIDFromCD);

			scheduler->DeclareReady();

            return scheduler;
        }
}