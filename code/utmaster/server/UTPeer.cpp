#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "UTPeer.h"
#include "UTDriver.h"
#include "UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	const CommandEntry Peer::m_client_commands[] = {
		CommandEntry(EClientIncomingRequest_ServerList, &Peer::handle_request_server_list),
		CommandEntry(EClientIncomingRequest_MOTD, &Peer::handle_motd),
		CommandEntry(-1, NULL)
	};

	const CommandEntry Peer::m_server_commands[] = {
		CommandEntry(EServerIncomingRequest_Heartbeat, &Peer::handle_heartbeat),
		CommandEntry(EServerIncomingRequest_StatsUpdate, &Peer::handle_stats_update),
		CommandEntry(EServerIncomingRequest_NewServer, &Peer::handle_newserver_request),
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
		if (current_time.tv_sec - m_last_recv.tv_sec > UTMASTER_PING_TIME) {
			Delete(true);
		} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(OS::Buffer recv_buffer) {
		uint8_t req_type;
			
		size_t len = (size_t)recv_buffer.ReadInt();
		OS::Buffer parse_buffer;
		if(len == 0 || len > recv_buffer.readRemaining()) return; //XXX: handle stream
		parse_buffer.WriteBuffer(recv_buffer.GetReadCursor(), len);
		recv_buffer.SetReadCursor(len);
		recv_buffer.SetWriteCursor(len);

		switch(m_state) {
			case EConnectionState_WaitChallengeResponse:
				handle_challenge_response(parse_buffer);
			break;
			case EConnectionState_WaitApprovedResponse:
				if (!m_config) { //shouldn't make it this far..
					Delete();
					return;
				}
				m_state = EConnectionState_WaitRequest;
				if (m_config->is_server) {
					handle_uplink_info(parse_buffer);
				}
				else {
					send_verified();
				}
					
			break;
			case EConnectionState_WaitRequest:
				req_type = parse_buffer.ReadByte();
				const CommandEntry *entry = GetCommandByCode(req_type);
				if(entry) {
					(*this.*entry->callback)(parse_buffer);
				} else {
					OS::LogText(OS::ELogLevel_Info, "[%s] Got unhandled request type %d", getAddress().ToString().c_str(), req_type);
					//Delete();
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
		//struct timeval current_time;

		//gettimeofday(&current_time, NULL);
		//if(current_time.tv_sec - m_last_ping.tv_sec > UTMASTER_PING_TIME) {
		//	gettimeofday(&m_last_ping, NULL);
		//	OS::Buffer send_buffer;
		//	send_buffer.WriteInt(0);	
		//	send_packet(send_buffer);
		//}

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

	int Peer::Read_CompactInt(OS::Buffer &buffer)
	{
		int Value = 0;

		char B[5];

		B[0] = buffer.ReadByte();
		if ((B[0] & 0x40) != 0)
		{
			B[1] = buffer.ReadByte();
			if ((B[1] & 0x80) != 0)
			{
				B[2] = buffer.ReadByte();
				if ((B[2] & 0x80) != 0)
				{
					B[3] = buffer.ReadByte();
					if ((B[3] & 0x80) != 0)
					{
						B[4] = buffer.ReadByte();
						Value = B[4];
					}
					Value = (Value << 7) + (B[3] & 0x7f);
				}
				Value = (Value << 7) + (B[2] & 0x7f);
			}
			Value = (Value << 7) + (B[1] & 0x7f);
		}
		Value = (Value << 6) + (B[0] & 0x3f);
		if ((B[0] & 0x80) != 0)
			Value = -Value;
		return Value;
	}

	void Peer::Write_CompactInt(OS::Buffer& buffer, int value) {
		int abs_val = abs(value);
		uint8_t B0 = ((value >= 0) ? 0 : 0x80) + ((abs_val < 0x40) ? abs_val : ((abs_val & 0x3f) + 0x40));
		buffer.WriteByte(B0);

		if (B0 & 0x40)
		{
			abs_val >>= 6;
			uint8_t B1 = (abs_val < 0x80) ? abs_val : ((abs_val & 0x7f) + 0x80);

			buffer.WriteByte(B1);
			if (B1 & 0x80)
			{
				abs_val >>= 7;
				uint8_t B2 = (abs_val < 0x80) ? abs_val : ((abs_val & 0x7f) + 0x80);
				buffer.WriteByte(B2);

				if (B2 & 0x80)
				{
					abs_val >>= 7;
					uint8_t B3 = (abs_val < 0x80) ? abs_val : ((abs_val & 0x7f) + 0x80);;
					buffer.WriteByte(B3);
					if (B3 & 0x80)
					{
						abs_val >>= 7;
						uint8_t B4 = abs_val;
						buffer.WriteByte(B4);
					}
				}
			}
		}
	}

	std::string Peer::Read_FString(OS::Buffer &buffer, bool skip_colours) {
		int length = Read_CompactInt(buffer);
		std::string result;
		if (length <= buffer.readRemaining()) {
			for (int i = 0; i < length; i++) {
				char ch = buffer.ReadByte();
				//skip null byte to fix comparision (such as client config lookup)
				if (i == length - 1 && ch == 0) {
					break;
				}
				if (skip_colours && ch == 0x1B) { //skip colour codes
					i += 3;
					buffer.ReadByte(); buffer.ReadByte(); buffer.ReadByte();
					continue;
				}

				result += ch;
			}
		}
		return result;
	}

	void Peer::Write_FString(std::string input, OS::Buffer &buffer) {
		Write_CompactInt(buffer, input.length() + 1);
		buffer.WriteBuffer(input.c_str(), input.length() + 1);
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