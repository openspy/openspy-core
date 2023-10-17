#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <OS/gamespy/gamespy.h>
#include <OS/gamespy/gsmsalg.h>

#include <sstream>

#include <OS/GPShared.h>

#include "SMPeer.h"
#include "SMDriver.h"
#include "SMServer.h"

namespace SM {
	const char *Peer::mp_hidden_str = "[hidden]";
	Peer::Peer(Driver *driver, uv_tcp_t *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		
	}
	void Peer::OnConnectionReady() {
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", getAddress().ToString().c_str());
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
	}
	void Peer::on_stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {	
		/*
		This scans the incoming packets for \\final\\ and splits based on that,


		as well as handles incomplete packets -- due to TCP preserving data order, this is possible, and cannot be used on UDP protocols
		*/
		std::string recv_buf = m_kv_accumulator;
		m_kv_accumulator.clear();
		recv_buf.append((const char *)buf->base, nread);

		size_t final_pos = 0, last_pos = 0;

		do {
			final_pos = recv_buf.find("\\final\\", last_pos);
			if (final_pos == std::string::npos) break;

			std::string partial_string = recv_buf.substr(last_pos, final_pos - last_pos);
			handle_packet(partial_string);

			last_pos = final_pos + 7; // 7 = strlen of \\final
		} while (final_pos != std::string::npos);


		//check for extra data that didn't have the final string -- incase of incomplete data
		if (last_pos < (size_t)nread) {
			std::string remaining_str = recv_buf.substr(last_pos);
			m_kv_accumulator.append(remaining_str);
		}

		if (m_kv_accumulator.length() > MAX_UNPROCESSED_DATA) {
			Delete();
			return;
		}
	}
	void Peer::think(bool packet_waiting) {
		if (m_delete_flag) return;

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > SM_PING_TIME * 2) {
			Delete(true);
		}
	}
	void Peer::handle_packet(std::string data) {
		OS::LogText(OS::ELogLevel_Debug, "[%s] Recv: %s\n", getAddress().ToString().c_str(), data.c_str());

		OS::KVReader data_parser = OS::KVReader(data);
		std::string command = data_parser.GetKeyByIdx(0);
		if(!command.compare("search")) {
			handle_search(data_parser);
		} else if(!command.compare("others")) {
			handle_others(data_parser);
		} else if(!command.compare("otherslist")) {
			handle_otherslist(data_parser);
		} else if(!command.compare("valid")) {
			handle_valid(data_parser);
		} else if(!command.compare("nicks")) {
			handle_nicks(data_parser);
		} else if(!command.compare("pmatch")) {

		} else if(!command.compare("check")) {
			handle_check(data_parser);
		} else if(!command.compare("newuser")) {
			handle_newuser(data_parser);
		} else if( !command.compare("searchunique")) {
			handle_searchunique(data_parser);
		} else if(!command.compare("profilelist")) {
			
		} else if(!command.compare("uniquesearch")) {
			handle_uniquesearch(data_parser);
		}
		
		gettimeofday(&m_last_recv, NULL);
	}

	void Peer::send_error(GPShared::GPErrorCode code, std::string addon_data) {
		GPShared::GPErrorData error_data = GPShared::getErrorDataByCode(code);
		if (error_data.msg == NULL) {
			Delete();
			return;
		}
		std::ostringstream ss;
		ss << "\\error\\";
		ss << "\\err\\" << error_data.error;
		if (error_data.die) {
			ss << "\\fatal\\";
		}
		ss << "\\errmsg\\" << error_data.msg;
		if (addon_data.length())
			ss << addon_data;
		
		SendPacket(ss.str().c_str());
		if (error_data.die) {
			Delete();
		}
	}
	void Peer::SendPacket(std::string string, bool attach_final) {
		const char *str = string.c_str();
		OS::Buffer buffer((void *)str, string.length());
		append_send_buffer(buffer);
	}
	void Peer::Delete(bool timeout) {
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
}