#include <tasks/tasks.h>
namespace MM {
    bool PerformDeleteServer(MMPushRequest request, TaskThreadData  *thread_data) {
        DeleteServer(thread_data, request.server, true);
        return true;
    }
	void DeleteServer(TaskThreadData *thread_data, MM::ServerInfo server, bool publish) {

    }
}