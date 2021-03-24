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
	void Peer::handle_getprofile(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		if (data_parser.HasKey("profileid") && data_parser.HasKey("id")) {
			int profileid = data_parser.GetValueInt("profileid");
			request.profile_search_details.id = profileid;
			request.extra = (void *)data_parser.GetValueInt("id");
			request.peer = this;
			request.peer->IncRef();
			request.callback = Peer::m_getprofile_callback;
			request.type = TaskShared::EProfileSearch_Profiles;

			TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
			scheduler->AddRequest(request.type, request);
		}
	}
    void Peer::m_getprofile_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		if(error_details.response_code != TaskShared::WebErrorCode_Success) {
			((GP::Peer *)peer)->send_error(GP_GETPROFILE);
			return;
		}
		if(results.size() == 0) {
			((GP::Peer *)peer)->send_error(GP_GETPROFILE_BAD_PROFILE);
			return;
		}
		std::vector<OS::Profile>::iterator it = results.begin();
		while(it != results.end()) {
			OS::Profile p = *it;
			OS::User user = result_users[p.userid];
			std::ostringstream s;


			s << "\\pi\\";
			s << "\\profileid\\" << p.id;
			s << "\\userid\\" << p.userid;

			if(p.nick.length()) {
				s << "\\nick\\" << p.nick;
			}

			if(p.uniquenick.length()) {
				s << "\\uniquenick\\" << p.uniquenick;
			}
			if(user.publicmask & GP_MASK_EMAIL || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				if(user.email.length()) {
					s << "\\email\\" << user.email;
				}
			}

			if(p.firstname.length()) {
				s << "\\firstname\\" << p.firstname;
			}

			if(p.lastname.length()) {
				s << "\\lastname\\" << p.lastname;
			}

			if(user.publicmask & GP_MASK_HOMEPAGE || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				if(p.homepage.length()) {
					s << "\\homepage\\" << p.homepage;
				}
			}

			if(user.publicmask & GP_MASK_SEX || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				s << "\\sex\\" << p.sex;
			}

			if(p.icquin) {
				s << "\\icquin\\" << p.icquin;
			}

			if(user.publicmask & GP_MASK_ZIPCODE || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				if(p.zipcode) {
					s << "\\zipcode\\" << p.zipcode;
				}
			}

			if(p.pic) {
				s << "\\pic\\" << p.pic;
			}

			if(p.ooc) {
				s << "\\ooc\\" << p.ooc;
			}

			if(p.ind) {
				s << "\\ind\\" << p.ind;
			}

			if(p.mar) {
				s << "\\pic\\" << p.mar;
			}

			if(user.publicmask & GP_MASK_BIRTHDAY || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				s << "\\birthday\\" << p.birthday.GetGPDate();
			}

			if(user.publicmask & GP_MASK_COUNTRYCODE || ((GP::Peer *)peer)->m_profile.userid == user.id) {
				s << "\\countrycode\\" << p.countrycode;
			}
			s << "\\aim\\" << p.aim;

			s << "\\videocard1string\\" << p.videocardstring[0];
			s << "\\videocard2string\\" << p.videocardstring[1];
			s << "\\osstring\\" << p.osstring;


			s << "\\id\\" << (int)extra;

			s << "\\sig\\d41d8cd98f00b204e9800998ecf8427e"; //temp until calculation fixed

			((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
			it++;
		}
	}
}