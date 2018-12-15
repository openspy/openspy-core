#include <server/SBPeer.h>
#include <tasks/tasks.h>
#include <sstream>
namespace MM {
    bool PerformSubmitData(MMQueryRequest request, TaskThreadData  *thread_data) {
		#if HACKER_PATCH_MSG_FORCE_NATNEG_ONLY
				request.buffer.resetReadCursor();
				if (request.buffer.readRemaining() != 10) {
					OS::LogText(OS::ELogLevel_Warning, "[%s] Rejecting non-10 submit data (%d)", request.peer->getAddress().ToString().c_str(), request.buffer.readRemaining());
					return true;
				}
				if (memcmp(request.buffer.GetHead(), "\xFD\xFC\x1E\x66\x6A\xB2", 6) != 0) {
					OS::LogText(OS::ELogLevel_Warning, "[%s] Rejecting non-natneg submit data", request.peer->getAddress().ToString().c_str());
					return true;
				}
		#endif

		const char *base64 = OS::BinToBase64Str((uint8_t *)request.buffer.GetReadCursor(), request.buffer.readRemaining());
		std::string b64_string = base64;
		free((void *)base64);

		std::string src_ip = request.from.ToString(true), dst_ip = request.to.ToString(true);


		std::ostringstream message;
		message << "\\send_msg\\REMOVED\\" << src_ip << "\\" <<
			request.from.GetPort() << "\\" <<
			dst_ip << "\\" <<
			request.to.GetPort() << "\\" <<
			b64_string;
		thread_data->mp_mqconnection->sendMessage(MM::mm_channel_exchange, MM::mm_client_message_routingkey, message.str());

		if(request.peer) {
			request.peer->DecRef();
		}
		return true;
    }
}