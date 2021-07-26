#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_acct_get_telemetry_token(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetTelemetryToken\n";
		if(kv_list.HasKey("TID")) {
			s << "TID=" << kv_list.GetValueInt("TID") << "\n";
		}
		s << "telemetryToken=\"teleToken\"\n";
		s << "enabled=0\n";
		s << "disabled=1\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
}