#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>


#include "GPPeer.h"
#include "GPDriver.h"
#include "GPServer.h"

/*
	TODO: delete/create profile errors
	cannot delete last profile
*/

namespace GP {

	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		m_got_buddies = false;
		m_got_blocks = false;
		mp_mutex = OS::CreateMutex();
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		gettimeofday(&m_status_refresh, NULL);

		m_status.status = GP_OFFLINE;
		m_status.status_str[0] = 0;
		m_status.location_str[0] = 0;
		m_status.quiet_flags = GP_SILENCE_NONE;
		m_status.address = sd->address;

		RegisterCommands();

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", m_sd->address.ToString().c_str());

		OS::gen_random(m_challenge, CHALLENGE_LEN);
		mp_proto_processor = new KVProcessor();

		send_login_challenge(1);
	}
	Peer::~Peer() {
		delete mp_proto_processor;
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
	}
	void Peer::Delete(bool timeout) {
		if(m_profile.id != 0) {
			GPBackendRedisRequest req;
			req.type = EGPRedisRequestType_UpdateStatus;
			req.peer = this;
			req.peer->IncRef();
			req.StatusInfo = GPShared::gp_default_status;
			req.extra = (void *)req.peer;

			TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
			scheduler->AddRequest(req.type, req);
		}

		m_delete_flag = true;
		m_timeout_flag = timeout;
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

			OS::KVReader data_parser;
			if (mp_proto_processor->ProcessIncoming(recv_buffer, data_parser, this)) {
				handle_packet(data_parser);
			}
		}

	end:
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		/* ping doesn't exist in older games
		if (current_time.tv_sec - m_last_recv.tv_sec > GP_PING_TIME * 2) {
			Delete(true);
		} else */
		if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}

	void Peer::RegisterCommands() {
		std::vector<CommandEntry> commands;
		commands.push_back(CommandEntry("newuser", false, &Peer::handle_newuser));
		commands.push_back(CommandEntry("login", false, &Peer::handle_login));
		commands.push_back(CommandEntry("ka", false, &Peer::handle_keepalive));
		commands.push_back(CommandEntry("logout", false, &Peer::handle_logout));
		commands.push_back(CommandEntry("status", true, &Peer::handle_status));
		commands.push_back(CommandEntry("statusinfo", true, &Peer::handle_statusinfo));
		commands.push_back(CommandEntry("addbuddy", true, &Peer::handle_addbuddy));
		commands.push_back(CommandEntry("delbuddy", true, &Peer::handle_delbuddy));
		commands.push_back(CommandEntry("removeblock", true, &Peer::handle_removeblock));
		commands.push_back(CommandEntry("delblock", true, &Peer::handle_removeblock));
		commands.push_back(CommandEntry("addblock", true, &Peer::handle_addblock));
		commands.push_back(CommandEntry("revoke", true, &Peer::handle_revoke));
		commands.push_back(CommandEntry("authadd", true, &Peer::handle_authadd));
		commands.push_back(CommandEntry("getprofile", true, &Peer::handle_getprofile));
		commands.push_back(CommandEntry("bm", true, &Peer::handle_bm));
		commands.push_back(CommandEntry("pinvite", true, &Peer::handle_pinvite));
		commands.push_back(CommandEntry("newprofile", true, &Peer::handle_newprofile));
		commands.push_back(CommandEntry("delprofile", true, &Peer::handle_delprofile));
		commands.push_back(CommandEntry("registernick", true, &Peer::handle_registernick));
		commands.push_back(CommandEntry("registercdkey", true, &Peer::handle_registercdkey));
		commands.push_back(CommandEntry("updatepro", true, &Peer::handle_updatepro));
		commands.push_back(CommandEntry("updateui", true, &Peer::handle_updateui));
		m_commands = commands;
	}
	void Peer::handle_packet(OS::KVReader data_parser) {
		gettimeofday(&m_last_recv, NULL);

		OS::LogText(OS::ELogLevel_Debug, "[%s] (%d) Recv: %s\n", getAddress().ToString().c_str(), m_profile.id, data_parser.ToString().c_str());

		std::string command = data_parser.GetKeyByIdx(0);

		std::vector<CommandEntry>::iterator it = m_commands.begin();
		while (it != m_commands.end()) {
			CommandEntry entry = *it;
			if (command.compare(entry.name) == 0) {
				if (entry.login_required) {
					if (m_backend_session_key.length() == 0) {
						break;
					}
				}
				(*this.*entry.callback)(data_parser);
				break;
			}
			it++;
		}

		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_status_refresh.tv_sec > (GP_STATUS_EXPIRE_TIME / 2)) {
			gettimeofday(&m_status_refresh, NULL);
			//often called with keep alives
			if (m_profile.id) {
				TaskScheduler<GP::GPBackendRedisRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetGPTask();
				GPBackendRedisRequest req;
				req.type = EGPRedisRequestType_UpdateStatus;
				req.peer = this;
				req.peer->IncRef();
				req.StatusInfo = m_status;
				scheduler->AddRequest(req.type, req);
			}
		}
	}

	void Peer::handle_registernick(OS::KVReader data_parser) {
		
	}
	void Peer::handle_registercdkey(OS::KVReader data_parser) {

	}

	void Peer::handle_statusinfo(OS::KVReader data_parser) {

	}

	void Peer::handle_keepalive(OS::KVReader data_parser) {
		//std::ostringstream s;
		//s << "\\ka\\";
		//SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::handle_logout(OS::KVReader data_parser) {
		Delete();
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
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::SendPacket(const uint8_t *buff, size_t len) {
		OS::Buffer buffer;
		buffer.WriteBuffer((void *)buff, len);
		buffer.WriteBuffer((void *)"\\final\\", 7);

		OS::LogText(OS::ELogLevel_Debug, "[%s] (%d) Send: %s\n", getAddress().ToString().c_str(), m_profile.id, std::string((const char *)buff, len).c_str());

		NetIOCommResp io_resp;
		io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamSend(m_sd, buffer);
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
			SendPacket((const uint8_t *)ping_packet.c_str(),ping_packet.length());
		}
	}
	void Peer::send_error(GPErrorCode code, std::string addon_data) {
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
		SendPacket((const uint8_t *)ss.str().c_str(), ss.str().length());
		if (error_data.die) {
			Delete();
		}
	}
	void Peer::send_user_blocked(int from_profileid) {
		if(std::find(m_blocked_by.begin(), m_blocked_by.end(), from_profileid) == m_blocked_by.end()) {
			m_blocked_by.push_back(from_profileid);
			if(m_buddies.find(from_profileid) != m_buddies.end()) {
				inform_status_update(from_profileid, m_buddies[from_profileid], true);
			}
		}
	}
	void Peer::send_user_block_deleted(int from_profileid) {
		if(std::find(m_blocked_by.begin(), m_blocked_by.end(), from_profileid) != m_blocked_by.end()) {
			std::vector<int>::iterator it = m_blocked_by.begin();
			while(it != m_blocked_by.end()) {
				int pid = *it;
				if(pid == from_profileid) {
					m_blocked_by.erase(it);

					if(m_buddies.find(pid) != m_buddies.end()) {
						inform_status_update(pid, m_buddies[pid], true);
					}
					return;
				}
				it++;
			}
		}
	}
	int Peer::GetProfileID() {
		return m_profile.id;
	}
}