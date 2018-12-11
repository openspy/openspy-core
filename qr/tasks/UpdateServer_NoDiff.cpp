#include <tasks/tasks.h>
namespace MM {
    bool PerformDeleteMissingKeysAndUpdateChanged(MMPushRequest request, TaskThreadData  *thread_data) {
		DeleteServer(thread_data, request.server, false);
		PushServer(thread_data, request.server, false, request.server.id);
        return true;
    }
}