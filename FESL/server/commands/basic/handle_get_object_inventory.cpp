#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_dobj_get_object_inventory(OS::KVReader kv_list) {
		std::string kv_str = "TXN=GetObjectInventory\n"
			"ObjectInventory.[]=0\n";
		SendPacket(FESL_TYPE_DOBJ, kv_str);
		return true;
	}
}