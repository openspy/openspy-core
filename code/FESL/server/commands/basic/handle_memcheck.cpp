#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_fsys_memcheck_handler(OS::KVReader kv_list) {
		//send_memcheck(0);
		return true;
	}
}