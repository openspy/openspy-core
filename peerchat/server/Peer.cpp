#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>


#include "Driver.h"
#include "Server.h"
#include "Peer.h"
namespace Peerchat {
    Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
        RegisterCommands();
    }
    Peer::~Peer() {
        
    }

    void Peer::OnConnectionReady() {
        
    }
    void Peer::Delete(bool timeout = false) {
        
    }
    
    void Peer::think(bool packet_waiting) {
			NetIOCommResp io_resp;
			if (m_delete_flag) return;

			if (packet_waiting) {
				OS::Buffer recv_buffer;
				io_resp = this->GetDriver()->getNetIOInterface()->streamRecv(m_sd, recv_buffer);

				int len = io_resp.comm_len;

				if (len <= 0) {
					goto end;
				}

				std::string recv_buf;
				recv_buf.append((const char *)recv_buffer.GetHead(), len);

				std::vector<std::string> commands = OS::KeyStringToVector(recv_buf, false, '\n');
				std::vector<std::string>::iterator it = commands.begin();
				while(it != commands.end()) {
					std::string command_line = OS::strip_whitespace(*it);
					std::vector<std::string> command_items = OS::KeyStringToVector(command_line, false, ' ');

					std::string command = command_items.at(0);

					std::vector<CommandEntry>::iterator it2 = m_commands.begin();
					while (it2 != m_commands.end()) {
						CommandEntry entry = *it2;
						if (command.compare(entry.name) == 0) {
							if (entry.login_required) {
								if(m_user.id == 0) break;
							}
							(*this.*entry.callback)(command_items);
							break;
						}
						it2++;
					}					
					*it++;
				}
			}

		end:
			//send_ping();

			//check for timeout
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if (current_time.tv_sec - m_last_recv.tv_sec > PEERCHAT_PING_TIME * 2) {
				Delete(true);
			} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
				Delete();
			}
    }
    void Peer::handle_packet(OS::KVReader data_parser) {
        
    }

    int Peer::GetProfileID() {

    }

    void Peer::run_timed_operations() {
        
    }
    void Peer::SendPacket(const uint8_t *buff, size_t len) {
        
    }

    void Peer::RegisterCommands() {
			std::vector<CommandEntry> commands;
			commands.push_back(CommandEntry("NICK", false, &Peer::handle_nick));
			commands.push_back(CommandEntry("USER", false, &Peer::handle_user));
			m_commands = commands;
    }
}