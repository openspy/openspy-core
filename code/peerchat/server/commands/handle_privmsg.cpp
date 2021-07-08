#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>


#include <server/Driver.h>
#include <server/Server.h>
#include <server/Peer.h>

namespace Peerchat {
    void Peer::handle_message_command(std::string type, std::vector<std::string> data_parser) {
        std::string message = data_parser.at(2);
        std::string target = data_parser.at(1);

		bool do_combine = false;
        if(message[0] == ':') {
			do_combine = true;
            message = message.substr(1);
        }

		if (do_combine) {
			for (int i = 3; i < data_parser.size(); i++) {
				message = message.append(" ").append(data_parser.at(i));
			}
		}

        bool block_message = false;
        if(message.length() > 0 && ~GetOperFlags() & OPERPRIVS_CTCP) {
            if(message[0] == 0x01) { //possible ctcp
                if(type.compare("PRIVMSG") == 0) {
                    block_message = true;
                    //ACTION
                    if(message.length() >= 7) {
                        std::string action = message.substr(1,6);
                        if(action.compare("ACTION") == 0) {
                            block_message = false;
                        }                        
                    }
                }
            }
        }

        if(block_message) {
            return;
        }

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SendMessageToTarget;
        req.peer = this;
        req.profile = m_profile;
        req.user = m_user;
        req.summary = m_user_details;
        req.message = message;
        req.message_type = type;
        req.message_target = target;
        req.peer->IncRef();
        req.callback = NULL;
        scheduler->AddRequest(req.type, req); 
    }
    void Peer::handle_privmsg(std::vector<std::string> data_parser) {
        if(m_user_details.modeflags & EUserMode_Gagged) {
            return;
        }
        handle_message_command("PRIVMSG", data_parser);
    }
    void Peer::handle_notice(std::vector<std::string> data_parser) {
        if(m_user_details.modeflags & EUserMode_Gagged) {
            return;
        }
        handle_message_command("NOTICE", data_parser);
    }
    void Peer::handle_utm(std::vector<std::string> data_parser) {
        handle_message_command("UTM", data_parser);
    }
    void Peer::handle_atm(std::vector<std::string> data_parser) {
        handle_message_command("ATM", data_parser);
    }
}