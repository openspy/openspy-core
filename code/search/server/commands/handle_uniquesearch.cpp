#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <OS/gamespy/gamespy.h>
#include <OS/gamespy/gsmsalg.h>

#include <sstream>

#include <OS/GPShared.h>

#include <server/SMPeer.h>
#include <server/SMDriver.h>
#include <server/SMServer.h>

namespace SM {
    void Peer::m_uniquesearch_cb(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
        std::ostringstream s;
        s << "\\us\\" << results.size();
        std::vector<OS::Profile>::iterator it = results.begin();
		while(it != results.end()) {
            OS::Profile p = *it;
            s << "\\nick\\" << p.uniquenick;
            it++;
        }
        s << "\\usdone\\";
		((Peer *)peer)->SendPacket(s.str().c_str());

		((Peer *)peer)->Delete();
    }
    void Peer::handle_uniquesearch(OS::KVReader data_parser) {
        TaskShared::ProfileRequest request;
		request.type = TaskShared::EProfileSearch_SuggestUniquenick;
        if(data_parser.HasKey("preferrednick")) {
            request.profile_search_details.uniquenick = data_parser.GetValue("preferrednick");
        } else {
            send_error(GPShared::GP_PARSE);
            return;
        }
        if(data_parser.HasKey("namespaceid")) {
            request.profile_search_details.namespaceid = data_parser.GetValueInt("namespaceid");
        }

		request.extra = NULL;
		request.peer = this;
		IncRef();
		request.callback = Peer::m_uniquesearch_cb;
		AddProfileTaskRequest(request);
    }
}