#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <OS/gamespy/gamespy.h>
#include <OS/gamespy/gsmsalg.h>

#include <sstream>

#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
#include <OS/GPShared.h>

#include <server/SMPeer.h>
#include <server/SMDriver.h>
#include <server/SMServer.h>

namespace SM {
	void Peer::m_search_valid_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::User> results, void *extra, INetPeer *peer) {
		std::ostringstream s;

		s << "\\vr\\" << ((results.size() > 0) ? 1 : 0);

		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
	}
	void Peer::handle_valid(OS::KVReader data_parser) {
		TaskShared::UserRequest request;
		request.type = TaskShared::EUserRequestType_Search;
		if (data_parser.HasKey("userid")) {
			request.search_params.id = data_parser.GetValueInt("userid");
		}
		
		if (data_parser.HasKey("partnercode")) {
			request.search_params.partnercode = data_parser.GetValueInt("partnercode");
		}
		else if(data_parser.HasKey("partnerid")) {
			request.search_params.partnercode = data_parser.GetValueInt("partnerid");
		}

		if(data_parser.HasKey("email")) {
			request.search_params.email = data_parser.GetValue("email");
		}
		request.peer = this;
		IncRef();
		request.callback = Peer::m_search_valid_callback;
		TaskScheduler<TaskShared::UserRequest, TaskThreadData> *scheduler = ((SM::Server *)(GetDriver()->getServer()))->GetUserTask();
		scheduler->AddRequest(request.type, request);
	}
}