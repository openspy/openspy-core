#include <OS/OpenSpy.h>
#include <OS/Buffer.h>


#include "UTPeer.h"
#include "UTDriver.h"
#include "UTServer.h"

namespace UT {
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		m_state = EConnectionState_WaitChallengeResponse;
		m_is_server = false;
		
	}
	void Peer::OnConnectionReady() {
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", getAddress().ToString().c_str());
		send_challenge("308275962");
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
		//send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > UTMASTER_PING_TIME * 2) {
			Delete(true);
		} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(OS::Buffer recv_buffer) {
		
		ERequestType req_type;
		int len = recv_buffer.ReadInt();
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
				printf("got req type: %d\n", req_type);
				switch(req_type) {
					case ERequestType_ServerList:
						handle_request_server_list(recv_buffer);
					break;
					case ERequestType_MOTD:
						send_motd();
					break;
					case ERequestType_NewServer:
						send_keys_request();
					break;

				}
			break;
			case EConnectionState_Heartbeat:
			handle_heartbeat(recv_buffer);
			break;
		}

	}

	void Peer::handle_heartbeat(OS::Buffer buffer) {
		int unk1 = buffer.ReadInt(), unk2 = buffer.ReadInt();
		int num_keys = buffer.ReadInt();
		int unk3 = buffer.ReadInt();
		std::string hostname = buffer.ReadNTS();
		printf("hostname: %s\n", hostname.c_str());
		for(int i=0;i<num_keys;i++) {
			std::string field = Read_FString(buffer);
			std::string property = Read_FString(buffer);
			printf("%s = %s\n", field.c_str(), property.c_str());
		}
	}

	void Peer::send_keys_request() {
		m_state = EConnectionState_Heartbeat;
		OS::Buffer send_buffer;
		send_buffer.WriteInt(3); //some kind of state key?
		send_buffer.WriteByte(0);
		send_packet(send_buffer);
	}

	void Peer::handle_challenge_response(OS::Buffer buffer) {
		std::string cdkey_hash = Read_FString(buffer);
		std::string cdkey_response = Read_FString(buffer);
		std::string client = Read_FString(buffer);
		uint32_t running_version = (buffer.ReadInt());
		uint8_t running_os = (buffer.ReadByte()) - 3;
		std::string language = Read_FString(buffer);
//		uint64_t cpu_cycles;
		uint32_t gpu_device_id = (buffer.ReadInt());
		uint32_t vendor_id = (buffer.ReadInt());
		uint32_t cpu_cycles = (buffer.ReadInt());
		uint32_t running_gpu = (buffer.ReadInt());
		OS::LogText(OS::ELogLevel_Info, "[%s] Got Game Info: cdkey hash: %s, cd key response: %s, client: %s, version: %d, os: %d, language: %s, gpu device id: %04x, gpu vendor id: %04x, cpu cycles: %d, running cpu: %d", getAddress().ToString().c_str(), cdkey_hash.c_str(), cdkey_response.c_str(), client.c_str(), running_version, running_os, language.c_str(),
			gpu_device_id, vendor_id, cpu_cycles, running_gpu
		);

		//XXX: map client to game info
		if(client.compare("UT2K4SERVER") == 0) {
			m_is_server = true;
		}
		send_challenge_authorization();
	}

	void Peer::send_challenge(std::string challenge_string) {
		OS::Buffer send_buffer;
		Write_FString(challenge_string, send_buffer);
		send_packet(send_buffer);
	}
	void Peer::send_challenge_authorization() {
		if(m_is_server) {
			m_state = EConnectionState_WaitRequest;
		} else {
			m_state = EConnectionState_ApprovedResponse;
		}
		

		OS::Buffer send_buffer;
		Write_FString("APPROVED", send_buffer);
		send_buffer.WriteInt(3);		
		send_packet(send_buffer);
		
	}
	void Peer::send_verified() {
		OS::Buffer send_buffer;
		Write_FString("VERIFIED", send_buffer);
		send_packet(send_buffer);
	}
	void Peer::handle_request_server_list(OS::Buffer recv_buffer) {
		char num_filter_fileds = recv_buffer.ReadByte();
		printf("num_filter_fileds: %d\n", num_filter_fileds);
		for(int i=0;i<num_filter_fileds;i++) {
			int field_len = recv_buffer.ReadByte(); //skip string len
			if(field_len == 0 || field_len == 4) { ///xxxx?? why 4?? game bug?
				i--; //don't include blank field as part of query
				continue;
			}
			std::string field = recv_buffer.ReadNTS();
			int property_len = recv_buffer.ReadByte();
			if(property_len == 0) {
				i--;  //don't include blank field as part of query
				continue;
			}
			std::string property = recv_buffer.ReadNTS();
			printf("%s (%d) = %s (%d)\n", field.c_str(), field_len, property.c_str(), property_len);
		}

		//XXX: lookup servers
		OS::Buffer send_buffer;
		send_buffer.WriteInt(0);
		send_buffer.WriteByte(1);
		send_packet(send_buffer);
	}

std::string get_file_contents(std::string path) {
	std::string ret;
	FILE *fd = fopen(path.c_str(),"r");
	if(fd) {
		fseek(fd,0,SEEK_END);
		int len = ftell(fd);
		fseek(fd,0,SEEK_SET);

		char *str_data = (char *)malloc(len+1);
		fread(str_data, len, 1, fd);
		str_data[len] = 0;
		ret = str_data;
		free((void *)str_data);
	}
	fclose(fd);
	return ret;
}
	void Peer::send_motd() {
		OS::Buffer send_buffer;
		printf("**** SEND NEWS\n");

		std::string string = get_file_contents("/home/dev/code/openspy-core-v2/code/utmaster/openspy.txt");

		Write_FString(string, send_buffer);
		
		send_buffer.WriteInt(0); //??
		send_packet(send_buffer);
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
}