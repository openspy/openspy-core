#ifndef _PEERCHAT_TASKS_H
#define _PEERCHAT_TASKS_H
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

#include <server/Peer.h>

namespace Peerchat {
  enum EPeerchatRequestType {
			EPeerchatRequestType_ReserveNickname,
	};

  class TaskResponse {
			public:
				TaskShared::WebErrorDetails error_details;
  };
	typedef void(*TaskCallback)(TaskResponse response_data, INetPeer *peer);

	class PeerchatBackendRequest {
		public:
			int type;
			Peer *peer;
			std::string nick;

			TaskCallback callback;
	};

	bool Perform_ReserveNickname(PeerchatBackendRequest request, TaskThreadData *thread_data);

  TaskScheduler<PeerchatBackendRequest, TaskThreadData> *InitTasks(INetServer *server);
}
#endif //_MM_TASKS_H