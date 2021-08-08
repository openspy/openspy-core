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
    Server::~Server() {
      delete mp_peerchat_tasks;
      delete mp_profile_tasks;
      delete mp_user_tasks;
      delete mp_auth_tasks;
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
    void Server::OnUserMessage(std::string type, UserSummary from, UserSummary to, std::string message) {
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
    void Server::OnChannelMessage(std::string type, ChannelUserSummary from, ChannelSummary channel, std::string message, ChannelUserSummary target, bool includeSelf, int requiredChanFlags, int requiredOperFlags, int onlyVisibleTo) {
      std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver *)*it;
        driver->OnChannelMessage(type, from, channel, message, target, includeSelf, requiredChanFlags, requiredOperFlags, onlyVisibleTo);
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
    void Server::OnChannelBroadcast(std::string type, UserSummary target, std::map<int, int> channel_list, std::string message, bool includeSelf) {
      std::vector<INetDriver*>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver*) * it;
        driver->OnChannelBroadcast(type, target, channel_list, message, includeSelf);
        it++;
      }
    }
    void Server::OnUpdateChanUsermode(int channel_id, UserSummary user_summary, int new_modeflags) {
      std::vector<INetDriver*>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver*) * it;
        driver->OnSetUserChanModeFlags(user_summary.id, channel_id, new_modeflags);
        it++;
      }
    }
    void Server::OnKillUser(UserSummary user_summary, std::string reason) {
      std::vector<INetDriver*>::iterator it = m_net_drivers.begin();
      while (it != m_net_drivers.end()) {
        Peerchat::Driver *driver = (Peerchat::Driver*) * it;
        Peerchat::Peer *peer = driver->FindPeerByUserSummary(user_summary);
        if(peer) {
          peer->OnRemoteDisconnect(reason);
        }        
        it++;      
      }
    }
}