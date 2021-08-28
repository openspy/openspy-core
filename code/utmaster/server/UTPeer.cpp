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
		
		char type = recv_buffer.ReadByte();
		int len = ntohl(recv_buffer.ReadInt());
		printf("got type: %d, len: %d\n", type, len);
		switch(type) {
			case MESSAGE_CHALLENGE_RESPONSE:
				send_challenge_authorization();
			break;
			case MESSAGE_VERIFICATION_DATA:
				send_verified();
			break;
			case MESSAGE_NEWS_REQUEST:
				send_news();
				Delete();
			break;
			case MESSAGE_REQUEST_SERVER_LIST:
				handle_request_server_list(recv_buffer);
			break;
		}

	}

	void Peer::send_challenge(std::string challenge_string) {
		OS::Buffer send_buffer;
		send_buffer.WriteNTS(challenge_string);
		send_packet(MESSAGE_CHALLENGE, send_buffer);
	}
	void Peer::send_challenge_authorization() {
		OS::Buffer send_buffer;
		send_buffer.WriteNTS("APPROVED");
		send_buffer.WriteInt(5);
		send_packet(MESSAGE_CHALLENGE_AUTHORIZATION, send_buffer);
	}
	void Peer::send_verified() {
		OS::Buffer send_buffer;
		send_buffer.WriteNTS("VERIFIED");
		send_packet(MESSAGE_VERIFIED, send_buffer);
	}
	void Peer::handle_request_server_list(OS::Buffer recv_buffer) {
		char num_filter_fileds = recv_buffer.ReadByte();
		printf("num_filter_fileds: %d\n", num_filter_fileds);
		for(int i=0;i<num_filter_fileds;i++) {
			recv_buffer.ReadByte(); //skip string len
			std::string field = recv_buffer.ReadNTS();
			recv_buffer.ReadByte(); //skip string len
			std::string property = recv_buffer.ReadNTS();
			printf("%s = %s\n", field.c_str(), property.c_str());
		}
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
	void Peer::send_news() {
		OS::Buffer send_buffer;
		send_buffer.WriteByte(0xb0);

		std::string string = get_file_contents("/home/dev/code/openspy-core-v2/code/utmaster/openspy.txt");


		send_buffer.WriteByte(0x03);
		send_buffer.WriteByte(0x00);
		send_buffer.WriteByte(0x00);
		send_buffer.WriteByte(0x7a);
		
		send_buffer.WriteByte(0x16);
		send_buffer.WriteByte(0x1B);
		send_buffer.WriteByte(0xff);
		send_buffer.WriteByte(0xff);
		send_buffer.WriteByte(0xff);
		

		send_buffer.WriteNTS(string);
		send_buffer.WriteInt(0); //??
		
		NetIOCommResp io_resp = this->GetDriver()->getNetIOInterface()->streamSend(m_sd, send_buffer);
		if(io_resp.disconnect_flag || io_resp.error_flag) {
			OS::LogText(OS::ELogLevel_Info, "[%s] Send Exit: %d %d", getAddress().ToString().c_str(), io_resp.disconnect_flag, io_resp.error_flag);
			Delete();
		}
	}

	void Peer::send_packet(char type, OS::Buffer buffer) {
		OS::Buffer send_buffer;
		send_buffer.WriteByte(type);
		send_buffer.WriteInt(htonl((uint32_t)buffer.bytesWritten()));
		

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
}