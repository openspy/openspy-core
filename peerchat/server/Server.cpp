#include "Server.h"
#include "Driver.h"
#include <tasks/tasks.h>
namespace Peerchat {
    Server::Server() {
      mp_auth_tasks = TaskShared::InitAuthTasks(this);
      mp_user_tasks = TaskShared::InitUserTasks(this);
      mp_profile_tasks = TaskShared::InitProfileTasks(this);
      mp_peerchat_tasks = Peerchat::InitTasks(this);
    }
    void Server::init() {

    }
    void Server::tick() {
      std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        INetDriver *driver = *it;
        driver->think(false);
        it++;
      }
      NetworkTick();
    }
    void Server::shutdown() {

    }
    INetPeer *Server::findPeerByProfile(int profile_id, bool inc_ref) {
        return NULL;
    }
    void Server::OnUserMessage(std::string type, std::string from, std::string to, std::string message) {
      std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver *)*it;
        Peerchat::Peer *peer = driver->FindPeerByUserSummary(to);
        if(peer) {
          peer->OnRecvDirectMsg(from, message, type);
        }
        it++;
      }
    }
    void Server::SendUserMessageToVisibleUsers(std::string fromSummary, std::string messageType, std::string message, bool includeSelf) {
      std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver *)*it;
        driver->SendUserMessageToVisibleUsers(fromSummary, messageType, message, includeSelf);
        it++;
      }
    }
    void Server::OnChannelMessage(std::string type, std::string from, ChannelSummary channel, std::string message) {
      std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver *)*it;
        driver->OnChannelMessage(type, from, channel, message);
        it++;
      }
    }
    void Server::OnSetUserChannelKeys(ChannelSummary summary, UserSummary user_summary, OS::KVReader keys) {
      std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver *)*it;
        driver->OnSetUserChannelKeys(summary, user_summary, keys);
        it++;
      }
    }

    void Server::OnSetChannelKeys(ChannelSummary summary, OS::KVReader keys) {
      std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver *)*it;
        driver->OnSetChannelKeys(summary, keys);
        it++;
      }
    }
}