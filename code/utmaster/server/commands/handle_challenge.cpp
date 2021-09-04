#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	void Peer::handle_challenge_response(OS::Buffer buffer) {
		std::string cdkey_hash = Read_FString(buffer);
		std::string cdkey_response = Read_FString(buffer);
		std::string client = Read_FString(buffer);

		m_config = ((UT::Driver *)this->GetDriver())->FindConfigByClientName(client);
		if(m_config == NULL) {
			send_challenge_response("MODIFIED_CLIENT");
			Delete();
			return;
		}
		uint32_t running_version = (buffer.ReadInt());
		m_client_version = running_version;

		uint8_t running_os = 0;
		if(m_client_version >= 3000 || !m_config->is_server) {
			running_os = (buffer.ReadByte()) - 3;
		} else {
			uint32_t unk1 = buffer.ReadInt();
			uint8_t unk2 = buffer.ReadByte();
		}
		
		std::string language = Read_FString(buffer);
		uint32_t gpu_device_id = 0;
		uint32_t vendor_id = 0;
		uint32_t cpu_cycles = 0;
		uint32_t running_gpu = 0;
		if(m_client_version >= 3000) {
			gpu_device_id = (buffer.ReadInt());
			vendor_id = (buffer.ReadInt());
			cpu_cycles = (buffer.ReadInt());
			running_gpu = (buffer.ReadInt());
		}
		OS::LogText(OS::ELogLevel_Info, "[%s] Got Game Info: cdkey hash: %s, cd key response: %s, client: %s, version: %d, os: %d, language: %s, gpu device id: %04x, gpu vendor id: %04x, cpu cycles: %d, running cpu: %d", getAddress().ToString().c_str(), cdkey_hash.c_str(), cdkey_response.c_str(), client.c_str(), running_version, running_os, language.c_str(),
			gpu_device_id, vendor_id, cpu_cycles, running_gpu
		);


		
		send_challenge_authorization();
	}

	void Peer::send_challenge(std::string challenge_string) {
		OS::Buffer send_buffer;
		Write_FString(challenge_string, send_buffer);
		send_packet(send_buffer);
	}
	void Peer::send_challenge_response(std::string response) {
		OS::Buffer send_buffer;
		Write_FString(response, send_buffer);
		send_buffer.WriteInt(3);		
		send_packet(send_buffer);
	}
	void Peer::send_challenge_authorization() {
		if(m_config->is_server || m_client_version < 3000) {
			m_state = EConnectionState_WaitRequest;

		} else {
			m_state = EConnectionState_ApprovedResponse;
		}
		send_challenge_response("APPROVED");
		
	}
	void Peer::send_verified() {
		OS::Buffer send_buffer;
		Write_FString("VERIFIED", send_buffer);
		send_packet(send_buffer);
	}
}