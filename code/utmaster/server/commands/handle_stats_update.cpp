#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	void Peer::handle_stats_update(OS::Buffer buffer) {
		std::string stats_data = Read_FString(buffer);

		OS::LogText(OS::ELogLevel_Info, "[%s] Stats Update: %s", getAddress().ToString().c_str(), stats_data.c_str());
	}
}