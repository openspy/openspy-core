#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_subs_get_entitlement_by_bundle(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetEntitlementByBundle\n";

		if(kv_list.HasKey("TID")) {
			s << "TID=" << kv_list.GetValueInt("TID") << "\n";
		}
		s << "EntitlementByBundle.[]=0\n";
		SendPacket(FESL_TYPE_SUBS, s.str());
		return true;
	}
}