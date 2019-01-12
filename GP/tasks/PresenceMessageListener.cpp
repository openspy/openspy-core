#include <server/GPServer.h>
#include <server/GPPeer.h>
#include <tasks/tasks.h>
#include <sstream>
namespace GP {
    bool Handle_PresenceMessage(TaskThreadData *thread_data, std::string message) {
		GP::Server *server = (GP::Server *)thread_data->server;

		std::string msg_type, reason;
		int to_profileid, from_profileid;
		GP::Peer *peer = NULL;

		OS::KVReader reader = OS::KVReader(message);
		msg_type = reader.GetValue("type");
		if (msg_type.compare("add_request") == 0) {
			reason = reader.GetValue("reason");
			to_profileid = reader.GetValueInt("to_profileid");
			from_profileid = reader.GetValueInt("from_profileid");
			peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
			if (peer) {
				peer->send_add_buddy_request(from_profileid, reason.c_str());
				peer->DecRef();
			}
		}
		else if (msg_type.compare("authorize_add") == 0) {
			to_profileid = reader.GetValueInt("to_profileid");
			from_profileid = reader.GetValueInt("from_profileid");
			peer = (GP::Peer *)server->findPeerByProfile(from_profileid);
			if (peer) {
				peer->send_authorize_add(from_profileid, to_profileid, reader.GetValueInt("silent"));
				peer->DecRef();
			}

			peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
			if (peer) {
				peer->send_authorize_add(from_profileid, to_profileid, reader.GetValueInt("silent"));
				peer->DecRef();
			}
		}
		else if (msg_type.compare("status_update") == 0) {
			GPShared::GPStatus status;
			from_profileid = reader.GetValueInt("profileid");
			status.status = (GPShared::GPEnum)reader.GetValueInt("status");

			status.status_str = reader.GetValue("status_string");
			status.location_str = reader.GetValue("location_string");
			status.quiet_flags = (GPShared::GPEnum)reader.GetValueInt("quiet_flags");
			reason = reader.GetValue("ip");
			status.address.ip = htonl(inet_addr(OS::strip_quotes(reason).c_str()));
			status.address.port = (reader.GetValueInt("port"));
			server->InformStatusUpdate(from_profileid, status);
		}
		else if (msg_type.compare("del_buddy") == 0) {
			to_profileid = reader.GetValueInt("to_profileid");
			from_profileid = reader.GetValueInt("from_profileid");
			peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
			if (peer) {
				peer->send_revoke_message(from_profileid, 0);
				peer->DecRef();
			}
		}
		else if (msg_type.compare("buddy_message") == 0) {
			char type = reader.GetValueInt("msg_type");
			int timestamp = reader.GetValueInt("timestamp");
			to_profileid = reader.GetValueInt("to_profileid");
			from_profileid = reader.GetValueInt("from_profileid");
			reason = reader.GetValue("message");
			peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
			if (peer) {
				peer->send_buddy_message(type, from_profileid, timestamp, reason.c_str());
				peer->DecRef();
			}
		}
		else if (msg_type.compare("block_buddy") == 0) {
			to_profileid = reader.GetValueInt("to_profileid");
			from_profileid = reader.GetValueInt("from_profileid");
			peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
			if (peer) {
				peer->send_user_blocked(from_profileid);
				peer->DecRef();
			}
		}
		else if (msg_type.compare("del_block_buddy") == 0) {
			to_profileid = reader.GetValueInt("to_profileid");
			from_profileid = reader.GetValueInt("from_profileid");
			peer = (GP::Peer *)server->findPeerByProfile(to_profileid);
			if (peer) {
				peer->send_user_block_deleted(from_profileid);
				peer->DecRef();
			}
		}
        return true;
    }
}