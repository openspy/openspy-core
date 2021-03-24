#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_subs_get_entitlement_by_bundle(OS::KVReader kv_list) {
		std::string kv_str = "TXN=GetEntitlementByBundle\n"
			"EntitlementByBundle.[]=0\n";
		SendPacket(FESL_TYPE_SUBS, kv_str);
		return true;
	}
}