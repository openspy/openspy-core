#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	void Peer::send_packages_data(uint32_t version) {
		std::vector<UT::PackageItem> packages;

		//calculate packages data
		std::vector<UT::PackageItem>::iterator it = this->m_config->packages.begin();
		while (it != this->m_config->packages.end()) {
			UT::PackageItem item = *it;

			if (item.version > version) {
				packages.push_back(item);
			}
			it++;
		}

		if (packages.empty()) {
			return;
		}

		//send packages data
		OS::Buffer buffer;
		buffer.WriteByte(EServerOutgoingRequest_PackagesData);
		Write_CompactInt(buffer, packages.size());

		it = packages.begin();
		while (it != packages.end()) {
			UT::PackageItem item = *it;
			Write_FString(item.guid, buffer);
			Write_FString(item.hash, buffer);
			buffer.WriteInt(item.version);
			it++;
		}

		send_packet(buffer);

	}
	void Peer::handle_packages_version(OS::Buffer recv_buffer) {
		uint32_t version = recv_buffer.ReadInt();

		OS::LogText(OS::ELogLevel_Info, "[%s] Got packages version: %d", getAddress().ToString().c_str(), version);



		send_server_id(1234); //init stats backend, generate match id, for now not needed
		send_packages_data(version);
	}

}