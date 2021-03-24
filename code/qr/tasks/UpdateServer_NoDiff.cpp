#include <tasks/tasks.h>
#include <server/QRPeer.h>
namespace MM {
    bool PerformDeleteMissingKeysAndUpdateChanged(MMPushRequest request, TaskThreadData  *thread_data) {
		DeleteServer(thread_data, request.server, false);
		PushServer(thread_data, request.server, false, request.server.id);
		if(request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}