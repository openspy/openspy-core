#include "FESLPeer.h"
#include "FESLDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/Search/User.h>
#include <OS/Search/Profile.h>
#include <OS/Auth.h>

#include <sstream>

namespace FESL {
	void Peer::loginToPersona(std::string uniquenick) {
		std::ostringstream s;
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			if (profile.uniquenick.compare(uniquenick) == 0) {
				m_profile = profile;
				s << "TXN=LoginPersona\n";
				s << "lkey=" << m_session_key << "\n";
				s << "profileId=" << m_profile.id << "\n";
				s << "userId=" << m_user.id << "\n";
				SendPacket(FESL_TYPE_ACCOUNT, s.str());
				break;
			}
			it++;
		}
		mp_mutex->unlock();
	}
	bool Peer::m_acct_login_persona(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=NuLoginPersona\n";
		s << "lkey=" << 555<< "\n";
		s << "profileId=" << 10083 << "\n";
		s << "userId=" << 10029 << "\n";
		m_profile.id = 10083;
		m_user.id = 10029;
		SendPacket(FESL_TYPE_ACCOUNT, s.str());

		loginToPersona(kv_list.GetValue("name"));
		return true;
	}
	void Peer::send_personas() {
		std::ostringstream s;
		mp_mutex->lock();
		std::vector<OS::Profile>::iterator it = m_profiles.begin();
		s << "TXN=NuGetPersonas\n";
		s << "personas.[]=" << m_profiles.size() << "\n";
		int i = 0;
		while (it != m_profiles.end()) {
			OS::Profile profile = *it;
			s << "personas." << i++ << "=\"" << profile.uniquenick << "\"\n";
			it++;
		}
		mp_mutex->unlock();
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
	}
	bool Peer::m_acct_nulogin_handler(OS::KVReader kv_list) {
		OS::AuthTask::TryAuthEmailPassword(kv_list.GetValue("nuid"), OS_EA_PARTNER_CODE, kv_list.GetValue("password"), m_nulogin_auth_cb, NULL, 0, this);
		return true;
	}
	bool Peer::m_acct_get_personas(OS::KVReader kv_list) {
		if (!m_got_profiles) {
			m_pending_nuget_personas= true;
		}
		else {
			send_personas();
		}
		return true;
	}
	void Peer::m_nulogin_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {
		std::ostringstream s;
		if (success) {
			s << "TXN=NuLogin\n";
			s << "lkey=" << auth_data.session_key << "\n";
			((Peer *)peer)->m_session_key = auth_data.session_key;
			s << "displayName=" << profile.uniquenick << "\n";
			s << "userId=" << user.id << "\n";
			s << "profileId=" << profile.id << "\n";
			((Peer *)peer)->m_logged_in = true;
			((Peer *)peer)->m_user = user;
			((Peer *)peer)->m_profile = profile;
			((Peer *)peer)->SendPacket(FESL_TYPE_ACCOUNT, s.str());

			OS::ProfileSearchRequest request;
			request.type = OS::EProfileSearch_Profiles;
			request.user_search_details.id = user.id;
			request.peer = peer;
			peer->IncRef();
			request.callback = Peer::m_search_callback;
			OS::m_profile_search_task_pool->AddRequest(request);
		}
		else {
			((Peer *)peer)->SendError(FESL_TYPE_ACCOUNT, FESL_ERROR_AUTH_FAILURE, "NuLogin");
		}
	}
	bool Peer::m_acct_get_telemetry_token(OS::KVReader kv_list) {
		std::ostringstream s;
		s << "TXN=GetTelemetryToken\n";
		s << "telemetryToken=\"teleToken\"\n";
		s << "enabled=0\n";
		s << "disabled=1\n";
		SendPacket(FESL_TYPE_ACCOUNT, s.str());
		return true;
	}
}