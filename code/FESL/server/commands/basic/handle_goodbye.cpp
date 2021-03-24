#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_fsys_goodbye_handler(OS::KVReader kv_list) {
		m_delete_flag = true;
		return true;
	}
}