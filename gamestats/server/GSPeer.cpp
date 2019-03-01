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

	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		
		mp_mutex = OS::CreateMutex();

		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);

		m_user.id = 0;
		m_profile.id = 0;

		m_session_key = rand();

		OS::gen_random(m_challenge, CHALLENGE_LEN);

		m_getpd_wait_ctx.wait_index = 0;
		m_getpd_wait_ctx.top_index = 0;
		m_get_request_index = 0;
		m_getpd_wait_ctx.mutex = OS::CreateMutex();

		m_setpd_wait_ctx.wait_index = 0;
		m_setpd_wait_ctx.top_index = 0;
		m_set_request_index = 0;
		m_setpd_wait_ctx.mutex = OS::CreateMutex();

		m_getpid_request_index = 0;
		m_getpid_wait_ctx.wait_index = 0;
		m_getpid_wait_ctx.top_index = 0;
		m_getpid_wait_ctx.mutex = OS::CreateMutex();

		m_updgame_increfs = 0;
		m_last_authp_operation_id = 0;
		m_xor_index = 0;

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection",m_sd->address.ToString().c_str());

		send_login_challenge(1);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed", m_sd->address.ToString().c_str());
		delete m_getpd_wait_ctx.mutex;
		delete m_setpd_wait_ctx.mutex;
		delete m_getpid_wait_ctx.mutex;
		delete mp_mutex;
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
	void Peer::think(bool packet_waiting) {
		
		NetIOCommResp io_resp;
		if (m_delete_flag) return;
		if (packet_waiting) {
			OS::Buffer recv_buffer;
			io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamRecv(m_sd, recv_buffer);
			int len = io_resp.comm_len;

			if (len <= 0) {
				goto end;
			}

			gamespy3dxor((char *)recv_buffer.GetHead(), recv_buffer.bytesWritten());

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
		end:
		//send_ping();

		/*
		//check for timeout -- Removed: not implemented in most clients
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > GP_PING_TIME*2) {
			Delete(true);
		} else */if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
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

		buffer.resetReadCursor();

		NetIOCommResp io_resp;
		io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamSend(m_sd, send_buffer);
		if (io_resp.disconnect_flag || io_resp.error_flag) {
			Delete();
		}
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

		mp_mutex->lock();
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
		mp_mutex->unlock();
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
		mp_mutex->lock();
		if (index > wait_ctx.top_index) {
			wait_ctx.top_index = index;
		}
		if (index == wait_ctx.wait_index) {
			wait_ctx.buffer_map.erase(index);
			this->SendPacket(buffer);
			OS::CMutex::SafeIncr(&wait_ctx.wait_index);
		}
		else {
			wait_ctx.buffer_map[index] = OS::Buffer(buffer);
		}
		int top_index = wait_ctx.top_index;
		while (top_index > 0) {
			std::map<int, OS::Buffer>::iterator it = wait_ctx.buffer_map.find(wait_ctx.wait_index);
			if (it != wait_ctx.buffer_map.end()) {
				this->SendPacket((*it).second);
				OS::CMutex::SafeIncr(&wait_ctx.wait_index);
				wait_ctx.buffer_map.erase(it);
				top_index--;

			}
			else {
				break;
			}
		}
		mp_mutex->unlock();
	}
}
