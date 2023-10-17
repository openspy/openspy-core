#include <OS/OpenSpy.h>

#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::send_ping() {
		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > FESL_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			std::ostringstream s;
			s << "TXN=Ping\n";
			s << "TID=" << current_time.tv_sec << "\n";
			SendPacket(FESL_TYPE_FSYS, s.str(), 1);
		}
	}
	bool Peer::m_fsys_ping_handler(OS::KVReader kv_list) {
		return true;
	}
}