#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "UTPeer.h"
#include "UTDriver.h"
#include "UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	const CommandEntry Peer::m_client_commands[] = {
		CommandEntry(EClientModeRequest_ServerList, &Peer::handle_request_server_list),
		CommandEntry(EClientModeRequest_MOTD, &Peer::handle_motd),
		CommandEntry(-1, NULL)
	};

	const CommandEntry Peer::m_server_commands[] = {
		CommandEntry(EServerModeRequest_ServerInit, &Peer::handle_server_init),
		CommandEntry(EServerModeRequest_Heartbeat, &Peer::handle_heartbeat),
		CommandEntry(EServerModeRequest_PlayerQuery, &Peer::handle_player_query),
		CommandEntry(EServerModeRequest_NewServer, &Peer::handle_newserver_request),
		CommandEntry(-1, NULL)
	};
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		m_state = EConnectionState_WaitChallengeResponse;
		m_config = NULL;
		m_got_server_init = false;
		
	}
	void Peer::OnConnectionReady() {
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", getAddress().ToString().c_str());
		send_challenge("111111111");
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
	}
	void Peer::think(bool packet_waiting) {
		NetIOCommResp io_resp;
		if (m_delete_flag) return;

		if (packet_waiting) {
			OS::Buffer recv_buffer;
			io_resp = this->GetDriver()->getNetIOInterface()->streamRecv(m_sd, recv_buffer);

			int len = io_resp.comm_len;

			if (len <= 0) {
				goto end;
			}
			gettimeofday(&m_last_recv, NULL);

			handle_packet(recv_buffer);

		}

	end:
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		/*if (current_time.tv_sec - m_last_recv.tv_sec > UTMASTER_PING_TIME * 2) {
			Delete(true);
		} else */if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(OS::Buffer recv_buffer) {
		//XXX: clean this up... logic is confusing to follow
		uint8_t req_type;
		while(recv_buffer.readRemaining() > 0) {
			
			size_t len = (size_t)recv_buffer.ReadInt();
			OS::Buffer parse_buffer;
			if(len == 0 || len > recv_buffer.readRemaining()) break;
			parse_buffer.WriteBuffer(recv_buffer.GetReadCursor(), len);
			recv_buffer.SetReadCursor(len);
			recv_buffer.SetWriteCursor(len);

			//printf("got data in state %d - len %d\n", m_state, len);
			switch(m_state) {
				case EConnectionState_WaitChallengeResponse:
					handle_challenge_response(parse_buffer);
				break;
				case EConnectionState_ApprovedResponse:
					send_verified();
					m_state = EConnectionState_WaitRequest;
				break;
				case EConnectionState_WaitRequest:
					if(!m_config) { //shouldn't make it this far..
						Delete();
						return;
					}
					req_type = parse_buffer.ReadByte();
					const CommandEntry *entry = GetCommandByCode(req_type);
					if(entry) {
						(*this.*entry->callback)(parse_buffer);
					} else {
						OS::LogText(OS::ELogLevel_Info, "[%s] Got unhandled request type %d", getAddress().ToString().c_str(), req_type);
						//Delete();
					}
					
				break;
			}
		}

	}

	const CommandEntry *Peer::GetCommandByCode(uint8_t code) {
		const CommandEntry *table = (const CommandEntry *)(&Peer::m_client_commands);
		if(m_config->is_server) {
			table =  (const CommandEntry *)(&Peer::m_server_commands);
		}
		int idx = 0;
		while(true) {
			if(table[idx].code == -1 || table[idx].callback == NULL) break;
			if(table[idx].code == code) return &table[idx];
			idx++;
		}
		return NULL;
	}

	void Peer::send_ping() {
		struct timeval current_time;

		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > UTMASTER_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			OS::Buffer send_buffer;
			send_buffer.WriteInt(0);	
			send_packet(send_buffer);
		}

	}


	void Peer::send_packet(OS::Buffer buffer) {
		OS::Buffer send_buffer;
		send_buffer.WriteInt((uint32_t)buffer.bytesWritten());
		

		send_buffer.WriteBuffer((char *)buffer.GetHead(), buffer.bytesWritten());
		NetIOCommResp io_resp = this->GetDriver()->getNetIOInterface()->streamSend(m_sd, send_buffer);
		if(io_resp.disconnect_flag || io_resp.error_flag) {
			OS::LogText(OS::ELogLevel_Info, "[%s] Send Exit: %d %d", getAddress().ToString().c_str(), io_resp.disconnect_flag, io_resp.error_flag);
			Delete();
		}
	}

	void Peer::Delete(bool timeout) {
		delete_server();

		m_timeout_flag = timeout;
		m_delete_flag = true;
		
	}

	std::string Peer::Read_FString(OS::Buffer &buffer) {
		uint8_t length = buffer.ReadByte();
		std::string result;
		if(length > 0) {
			result = buffer.ReadNTS();
		}
		return result;
	}

	void Peer::Write_FString(std::string input, OS::Buffer &buffer) {
		buffer.WriteByte(input.length() + 1);
		buffer.WriteNTS(input);
	}
	int Peer::GetGameId() {
		if(m_config == NULL) return 0;
		return m_config->gameid;
	}
	void Peer::delete_server() {
		if(m_config != NULL && m_config->is_server) {
			TaskScheduler<MM::UTMasterRequest, TaskThreadData> *scheduler = ((UT::Server *)(this->GetDriver()->getServer()))->getScheduler();
			MM::UTMasterRequest req;        
			req.type = MM::UTMasterRequestType_DeleteServer;
			req.peer = this;
			req.peer->IncRef();
			req.record.m_address = m_server_address;
			req.callback = NULL;
			scheduler->AddRequest(req.type, req);
		}
	}
}