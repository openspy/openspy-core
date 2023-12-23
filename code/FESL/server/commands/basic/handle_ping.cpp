#include <OS/OpenSpy.h>

#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::send_ping() {
		//check for timeout
		uv_timespec64_t current_time;
		uv_clock_gettime(UV_CLOCK_MONOTONIC, &current_time);
		if(current_time.tv_sec - m_last_ping.tv_sec > FESL_PING_TIME) {
			uv_clock_gettime(UV_CLOCK_MONOTONIC, &m_last_ping);
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