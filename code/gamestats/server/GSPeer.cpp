#include "GSServer.h"
#include "GSPeer.h"
#include "GSDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <stddef.h>
#include <sstream>
#include <algorithm>

#include <server/tasks/tasks.h>

#include <OS/GPShared.h>

#include <map>
#include <utility>
using namespace GPShared;

namespace GS {

	Peer::Peer(Driver *driver, uv_tcp_t *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		
		uv_mutex_init(&m_mutex);

		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);

		m_user.id = 0;
		m_profile.id = 0;

		m_session_key = m_last_ping.tv_sec;

		OS::gen_random(m_challenge, CHALLENGE_LEN);

		m_getpd_wait_ctx.wait_index = 0;
		m_getpd_wait_ctx.top_index = 0;
		m_get_request_index = 0;
		uv_mutex_init(&m_getpd_wait_ctx.mutex);

		m_setpd_wait_ctx.wait_index = 0;
		m_setpd_wait_ctx.top_index = 0;
		m_set_request_index = 0;
		uv_mutex_init(&m_setpd_wait_ctx.mutex);

		m_getpid_request_index = 0;
		m_getpid_wait_ctx.wait_index = 0;
		m_getpid_wait_ctx.top_index = 0;
		uv_mutex_init(&m_getpid_wait_ctx.mutex);

		m_updgame_increfs = 0;
		m_last_authp_operation_id = 0;
		m_xor_index = 0;
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed", getAddress().ToString().c_str());
		uv_mutex_destroy(&m_getpd_wait_ctx.mutex);
		uv_mutex_destroy(&m_setpd_wait_ctx.mutex);
		uv_mutex_destroy(&m_getpid_wait_ctx.mutex);
		uv_mutex_destroy(&m_mutex);
	}
	void Peer::OnConnectionReady() {
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection",getAddress().ToString().c_str());

		send_login_challenge(1);
	}
	void Peer::send_login_challenge(int type) {
		std::ostringstream s;

		s << "\\lc\\" << type;
		switch(type) {
			case 1: {
				s << "\\challenge\\" << m_challenge;
				s << "\\id\\" << GPI_CONNECTING;
				break;
			}
		}
		SendPacket(s.str());
	}
	void Peer::think() {
		
		if (m_delete_flag) return;
		send_ping();
	}
	void Peer::handle_packet(std::string packet_string) {
		OS::LogText(OS::ELogLevel_Debug, "[%s] Recv: %s", getAddress().ToString().c_str(), packet_string.c_str());


		std::map<std::string, std::string> data_map;
		data_map["data"] = "length";
		OS::KVReader data_parser = OS::KVReader(packet_string, '\\', 0, data_map);
		
		if(data_parser.Size() == 0) {
			return;
		}
		gettimeofday(&m_last_recv, NULL);

		std::string command = data_parser.GetKeyByIdx(0);
		if(command.compare("auth") == 0) {
			handle_auth(data_parser);
		} else if(command.compare("authp") == 0) {
			handle_authp(data_parser);
		} else if(command.compare("ka") == 0) { //keep alive

		} else if(command.compare("newgame") == 0) {
			handle_newgame(data_parser);
		} else if(command.compare("updgame") == 0) {
			handle_updgame(data_parser);
		} else if(command.compare("getpid") == 0) {
			handle_getpid(data_parser);
		} else if (command.compare("getpd") == 0) {
			handle_getpd(data_parser);
		}

		if(m_user.id != 0) {
			if(command.compare("setpd") == 0) {
				handle_setpd(data_parser);
			}
		}
	}

	void Peer::SendPacket(std::string str, bool attach_final) {
		OS::Buffer buffer(str.length());
		buffer.WriteBuffer((void *)str.c_str(), str.length());
		SendPacket(buffer, attach_final);
	}
	void Peer::SendPacket(OS::Buffer &buffer, bool attach_final) {
		OS::Buffer send_buffer;
		send_buffer.WriteBuffer((char *)buffer.GetHead(), buffer.bytesWritten());
		if (attach_final) {
			send_buffer.WriteBuffer((void *)"\\final\\", 7);
		}
		OS::LogText(OS::ELogLevel_Debug, "[%s] Send: %s", getAddress().ToString().c_str(), std::string((const char *)buffer.GetHead(), buffer.bytesWritten()).c_str());
		gamespy3dxor((char *)send_buffer.GetHead(), send_buffer.bytesWritten());


		append_send_buffer(send_buffer);

		// buffer.resetReadCursor();

		// NetIOCommResp io_resp;
		// io_resp = this->GetDriver()->getNetIOInterface()->streamSend(m_sd, send_buffer);
		// if (io_resp.disconnect_flag || io_resp.error_flag) {
		// 	Delete();
		// }
	}
	void Peer::send_ping() {
		//check for timeout
		struct timeval current_time;

		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > GP_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			std::string ping_packet = "\\ka\\";
			SendPacket(ping_packet);
		}
	}

	void Peer::send_error(GPShared::GPErrorCode code) {
		GPShared::GPErrorData error_data = GPShared::getErrorDataByCode(code);
		if(error_data.msg == NULL) {
			m_delete_flag = true;
			return;
		}
		std::ostringstream ss;
		ss << "\\error\\";
		ss << "\\err\\" << error_data.error;
		ss << "\\errmsg\\" << error_data.msg;
		if(error_data.die) {
			ss << "\\fatal\\" << error_data.die;
		}
		SendPacket(ss.str());
		if (error_data.die) {
			Delete();
		}
	}

	void Peer::gamespy3dxor(char *data, int len) {
	    static const char gamespy[] = "GameSpy3D\0";
	    char  *gs;

	    gs = (char *)((ptrdiff_t)gamespy) + m_xor_index;
	    while(len-- && len >= 0) {
			if(strncmp(data,"\\final\\",7) == 0)  {
				data+=7;
				len-=6;
				gs = (char *)gamespy;
				continue;
			}
	        *data++ ^= *gs++;
	        if(!*gs) gs = (char *)gamespy;
	    }
		m_xor_index = gs - ((char *)gamespy);
	}
	bool Peer::IsResponseValid(const char *response) {
		int chrespnum = gs_chresp_num(m_challenge);
		std::ostringstream s;
		s << chrespnum << m_game.secretkey;

		bool ret = false;

		const char *true_resp = OS::MD5String(s.str().c_str());

		ret = strcmp(response,true_resp) == 0;

		free((void *)true_resp);

		return ret;

	}
	int Peer::gs_chresp_num(const char *challenge) {
	    int     num = 0;

	    while(*challenge) {
	        num = *challenge - (num * 0x63306CE7);
	        challenge++;
	    }
	    return(num);
	}

	int Peer::GetProfileID() {
		return m_profile.id;
	}

	void Peer::Delete(bool timeout) {
		m_delete_flag = true;
		m_timeout_flag = timeout;
	}
	void Peer::SendOrWaitBuffer(uint32_t index, WaitBufferCtx &wait_ctx, OS::Buffer buffer) {
		uv_mutex_lock(&m_mutex);
		if (index > wait_ctx.top_index) {
			wait_ctx.top_index = index;
		}
		if (index == wait_ctx.wait_index) {
			wait_ctx.buffer_map.erase(index);
			this->SendPacket(buffer);
			wait_ctx.wait_index.fetch_add(1);
			

		}
		else {
			wait_ctx.buffer_map[index] = OS::Buffer(buffer);
		}
		int top_index = wait_ctx.top_index;
		while (top_index > 0) {
			std::map<int, OS::Buffer>::iterator it = wait_ctx.buffer_map.find(wait_ctx.wait_index);
			if (it != wait_ctx.buffer_map.end()) {
				this->SendPacket((*it).second);
				wait_ctx.wait_index.fetch_add(1);
				wait_ctx.buffer_map.erase(it);
				top_index--;

			}
			else {
				break;
			}
		}
		uv_mutex_unlock(&m_mutex);
	}
	void Peer::on_stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
		OS::Buffer recv_buffer;
		recv_buffer.WriteBuffer(buf->base, nread);
		recv_buffer.resetReadCursor();

		int len = recv_buffer.bytesWritten();
		gamespy3dxor((char *)recv_buffer.GetHead(), len);

		/*
			This scans the incoming packets for \\final\\ and splits based on that,
			
			
			as well as handles incomplete packets -- due to TCP preserving data order, this is possible, and cannot be used on UDP protocols				
		*/
		std::string recv_buf = m_kv_accumulator;
		m_kv_accumulator.clear();
		recv_buf.append((const char *)recv_buffer.GetHead(), len);

		size_t final_pos = 0, last_pos = 0;

		do {
			final_pos = recv_buf.find("\\final\\", last_pos);
			if (final_pos == std::string::npos) break;

			std::string partial_string = recv_buf.substr(last_pos, final_pos - last_pos);
			handle_packet(partial_string);

			last_pos = final_pos + 7; // 7 = strlen of \\final
		} while (final_pos != std::string::npos);


		//check for extra data that didn't have the final string -- incase of incomplete data
		if (last_pos < (size_t)len) {
			std::string remaining_str = recv_buf.substr(last_pos);
			m_kv_accumulator.append(remaining_str);
		}

		if (m_kv_accumulator.length() > MAX_UNPROCESSED_DATA) {
			Delete();
			return;
		}
	}


	void Peer::AddRequest(PersistBackendRequest req) {
		uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));

		PersistBackendRequest *work_data = new PersistBackendRequest();
		*work_data = req;

		uv_handle_set_data((uv_handle_t*) uv_req, work_data);

		uv_queue_work(uv_default_loop(), uv_req, PerformUVWorkRequest, PerformUVWorkRequestCleanup);
	}
}
