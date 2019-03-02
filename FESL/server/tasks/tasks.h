#ifndef _FESL_TASKS_H
#define _FESL_TASKS_H
#include <string>

#include <OS/Task/TaskScheduler.h>
#include <OS/Task/ScheduledTask.h>

#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>

#include <OS/SharedTasks/tasks.h>
#include <OS/SharedTasks/Auth/AuthTasks.h>

#include <curl/curl.h>
#include <jansson.h>

#include <OS/SharedTasks/Account/ProfileTasks.h>

#include <server/FESLServer.h>
#include <server/FESLPeer.h>

namespace FESL {
	typedef struct {
        int type;		
	} FESLRequest;

    bool Handle_AuthEvent(TaskThreadData *thread_data, std::string message);
    TaskScheduler<FESLRequest, TaskThreadData> *InitTasks(INetServer *server);
}
#endif //_MM_TASKS_H