#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "UTPeer.h"
#include "UTDriver.h"
#include "UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		m_state = EConnectionState_WaitChallengeResponse;
		m_config = NULL;
		
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
		
		ERequestType req_type;
		int len = recv_buffer.ReadInt();
		printf("got data in state %d - len %d\n", m_state, len);
		switch(m_state) {
			case EConnectionState_WaitChallengeResponse:
				handle_challenge_response(recv_buffer);
			break;
			case EConnectionState_ApprovedResponse:
				send_verified();
				m_state = EConnectionState_WaitRequest;
			break;
			case EConnectionState_WaitRequest:
				req_type = (ERequestType)recv_buffer.ReadByte();
				printf("wait req type: %d\n", req_type);
				switch(req_type) {
					case ERequestType_ServerList:
						handle_request_server_list(recv_buffer);
					break;
					case ERequestType_MOTD:
						send_motd();
					break;
					case ERequestType_NewServer:
						handle_newserver_request(recv_buffer);
						//send_server_id(1234);
					break;
				}
			break;
			case EConnectionState_Heartbeat:
			req_type = (ERequestType)recv_buffer.ReadByte() + HEARTBEAT_MODE_OFFSET;
			printf("hb req: %08X\n", req_type - HEARTBEAT_MODE_OFFSET);
			switch(req_type) {
				case ERequestType_ServerInit:
					send_server_id(0);
				case ERequestType_Heartbeat:
					handle_heartbeat(recv_buffer);
				break;
				case ERequestType_PlayerQuery:
					//printf("player query\n");
				break;
				case ERequestType_RequestMatchId:
					send_server_id(1234);
				break;
			}
			
			break;
		}
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