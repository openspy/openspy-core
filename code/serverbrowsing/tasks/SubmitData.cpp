#include <server/SBPeer.h>
#include "tasks.h"
#include <sstream>
namespace MM {
	bool PerformSubmitData(MMQueryRequest request, TaskThreadData  *thread_data) {
		std::string b64_string;
		const char *base64;
		std::ostringstream message;
		std::string src_ip, dst_ip;
		#if HACKER_PATCH_MSG_FORCE_NATNEG_ONLY
		if(request.req.m_for_game.gameid == 1420) { //only apply patch to flatout2 pc
				request.buffer.resetReadCursor();
				if (request.buffer.readRemaining() != 10) {
					OS::LogText(OS::ELogLevel_Warning, "[%s] Rejecting non-10 submit data (%d)", request.peer->getAddress().ToString().c_str(), request.buffer.readRemaining());
					goto exit_clean;
				}
				if (memcmp(request.buffer.GetHead(), "\xFD\xFC\x1E\x66\x6A\xB2", 6) != 0) {
					OS::LogText(OS::ELogLevel_Warning, "[%s] Rejecting non-natneg submit data", request.peer->getAddress().ToString().c_str());
					goto exit_clean;
				}
		}

		#endif

		base64 = OS::BinToBase64Str((uint8_t *)request.buffer.GetReadCursor(), request.buffer.readRemaining());
		b64_string = base64;
		free((void *)base64);

		src_ip = request.from.ToString(true), dst_ip = request.to.ToString(true);



		message << "\\send_msg\\REMOVED\\" << src_ip << "\\" <<
			request.from.GetPort() << "\\" <<
			dst_ip << "\\" <<
			request.to.GetPort() << "\\" <<
			b64_string;

		b64_string = message.str();
		TaskShared::sendAMQPMessage(MM::mm_channel_exchange, MM::mm_client_message_routingkey, b64_string.c_str());

		exit_clean:
		if(request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}