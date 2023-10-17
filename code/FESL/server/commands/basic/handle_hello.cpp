#include <OS/OpenSpy.h>

#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	bool Peer::m_fsys_hello_handler(OS::KVReader kv_list) {
		std::ostringstream ss;

		char timeBuff[128];
		struct tm *newtime;
		time_t long_time;
		time(&long_time);
		newtime = gmtime(&long_time);

		strftime(timeBuff, sizeof(timeBuff), FESL_DATE_FORMAT, newtime);

		PublicInfo public_info = ((FESL::Driver *)mp_driver)->GetServerInfo();
		ss << "TXN=Hello\n";
		if(kv_list.HasKey("TID")) {
			ss << "TID=" << kv_list.GetValueInt("TID") << "\n";
		}
		ss << "domainPartition.domain=" << public_info.domainPartition << "\n";
		ss << "messengerIp=" << public_info.messagingHostname << "\n";
		ss << "messengerPort=" << public_info.messagingPort << "\n";
		ss << "domationPartition.subDomain=" << public_info.subDomain << "\n";
		ss << "activityTimeoutSecs=" << FESL_PING_TIME * 2 << "\n";
		ss << "curTime=\"" << OS::url_encode(timeBuff) << "\"\n";
		ss << "theaterIp=" << public_info.theaterHostname << "\n";
		ss << "theaterPort=" << public_info.theaterPort << "\n";
		SendPacket(FESL_TYPE_FSYS, ss.str());

		send_memcheck(0);
		return true;
	}
}