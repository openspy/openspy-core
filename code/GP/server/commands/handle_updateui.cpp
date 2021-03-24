#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>

#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>

namespace GP {
	void Peer::handle_updateui(OS::KVReader data_parser) {
		TaskShared::UserRequest request;

		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		
		LoadParamInt("cpubrandid", m_user.cpu_brandid, data_parser)
		LoadParamInt("cpuspeed", m_user.cpu_speed, data_parser)
		LoadParamInt("vidocard1ram", m_user.videocard_ram[0], data_parser)
		LoadParamInt("vidocard2ram", m_user.videocard_ram[1], data_parser)
		LoadParamInt("connectionid", m_user.connectionid, data_parser)
		LoadParamInt("connectionspeed", m_user.connectionspeed, data_parser)
		LoadParamInt("hasnetwork", m_user.hasnetwork, data_parser)
		LoadParam("email", m_user.email, data_parser)


		if(data_parser.HasKey("passwordenc")) {
			std::string pwenc = data_parser.GetValue("passwordenc");
			int passlen = (int)pwenc.length();
			char *dpass = (char *)base64_decode((uint8_t *)pwenc.c_str(), &passlen);
			passlen = gspassenc((uint8_t *)dpass);

			if(dpass)
				m_user.password = dpass;

			if(dpass)
				free((void *)dpass);
		}
		else if (data_parser.HasKey("password")) {
			m_user.password = data_parser.GetValue("password");
		}
		
		request.search_params = m_user;
		request.type = TaskShared::EUserRequestType_Update;
		request.extra = NULL;
		request.peer = this;
		//IncRef();
		request.callback = NULL;
		request.search_params = m_user;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *user_scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetUserTask();
		user_scheduler->AddRequest(request.type, request);
	}
}