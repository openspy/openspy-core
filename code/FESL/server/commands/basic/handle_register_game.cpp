#include <OS/OpenSpy.h>

#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_acct_register_game_handler(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=RegisterGame\n";
		if(kv_list.HasKey("TID")) {
			s << "TID=" << kv_list.GetValueInt("TID") << "\n";
		}
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
}