#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <peerchat/tasks/tasks.h>
#include <OS/SharedTasks/tasks.h>
#include "Driver.h"

#include <sstream>
namespace Peerchat {

    Driver::Driver(INetServer *server, std::string server_name, const char *host, uint16_t port, bool proxyHeaders) : TCPDriver(server, host, port, proxyHeaders) {
		m_server_name = server_name;
    }
    Peer *Driver::FindPeerByProfileID(int profileid) {
        return NULL;
    }
	bool Driver::LLIterator_FindPeerByUserSummary(INetPeer* peer, FindPeerByUserSummaryState* state) {
		Peer* p = (Peer *)peer;
		if (stricmp(p->GetUserDetails().ToString().c_str(), state->summary.ToString().c_str()) == 0 && !p->ShouldDelete()) {
			state->peer = p;
			return false;
		}
		return true;
	}
    Peer *Driver::FindPeerByUserSummary(UserSummary summary) {

		FindPeerByUserSummaryState state;
		state.summary = summary;
		state.peer = NULL;

		mp_mutex->lock();
		OS::LinkedListIterator<INetPeer*, FindPeerByUserSummaryState*> iterator(mp_peers);
    	iterator.Iterate(LLIterator_FindPeerByUserSummary, &state);
		mp_mutex->unlock();

		return state.peer;   
    }
    INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
        return new Peer(this, socket);
    }
	bool Driver::LLIterator_SendUserMessageToVisibleUsers(INetPeer* peer, SendMessageIteratorState* state) {
		Peer *p = (Peer *)peer;
		bool summaryMatch = p->GetUserDetails().ToString().compare(state->fromSummary);
		if(summaryMatch && state->includeSelf) {
			p->send_message(state->messageType, state->message, state->fromSummary);
		} else if(!summaryMatch) {
			p->send_message(state->messageType, state->message, state->fromSummary);
		}
		return true;
	}
	void Driver::SendUserMessageToVisibleUsers(std::string fromSummary, std::string messageType, std::string message, bool includeSelf) {
		SendMessageIteratorState state;
		state.fromSummary = fromSummary;
		state.messageType = messageType;
		state.message = message;
		state.includeSelf = includeSelf;

		mp_mutex->lock();
		OS::LinkedListIterator<INetPeer*, SendMessageIteratorState*> iterator(mp_peers);
    	iterator.Iterate(LLIterator_SendUserMessageToVisibleUsers, &state);
		mp_mutex->unlock();
	}
	bool Driver::LLIterator_OnChannelMessage(INetPeer* peer, SendMessageIteratorState* state)
	{
		Peer *p = (Peer *)peer;
		if (state->from.modeflags & EUserChannelFlag_Invisible) {
			if (~p->GetOperFlags() & OPERPRIVS_INVISIBLE) {
				return true;
			}
		}
		bool in_channel = p->GetChannelFlags(state->channel.channel_id) & EUserChannelFlag_IsInChannel && (p->GetChannelFlags(state->channel.channel_id) & state->requiredChanUserModes || state->requiredChanUserModes == 0);
		int has_oper = p->GetOperFlags() & state->requiredOperFlags || state->requiredOperFlags == 0;
		bool selfMatch = (state->includeSelf && state->from.user_id == p->GetBackendId()) || (state->from.user_id != p->GetBackendId());
		if(in_channel && selfMatch && has_oper) {
			if(state->onlyVisibleTo == 0 || state->onlyVisibleTo == p->GetBackendId()) {
				p->send_message(state->type, state->message, state->from.userSummary, state->channel.channel_name, state->target.userSummary.nick);
			}

			if (state->type.compare("JOIN") == 0 && state->from.user_id == p->GetBackendId()) {
				p->handle_channel_join_events(state->channel);
			}
		}
		return true;
	}
	void Driver::OnChannelMessage(std::string type, ChannelUserSummary from, ChannelSummary channel, std::string message, ChannelUserSummary target, bool includeSelf, int requiredChanUserModes, int requiredOperFlags, int onlyVisibleTo) {
		SendMessageIteratorState state;
		state.type = type;
		state.from = from;
		state.channel = channel;
		state.message = message;
		state.target = target;
		state.includeSelf = includeSelf;
		state.requiredChanUserModes = requiredChanUserModes;
		state.requiredOperFlags = requiredOperFlags;
		state.onlyVisibleTo = onlyVisibleTo;

		mp_mutex->lock();
		OS::LinkedListIterator<INetPeer*, SendMessageIteratorState*> iterator(mp_peers);
    	iterator.Iterate(LLIterator_OnChannelMessage, &state);
		mp_mutex->unlock();
	}
	bool Driver::LLIterator_OnSetUserChannelKeys(INetPeer* peer, SetChannelKeysIteratorState* state) {
		Peer *p = (Peer *)peer;
		if(p->GetChannelFlags(state->summary.channel_id) & EUserChannelFlag_IsInChannel) {
			p->send_numeric(702,state->keys_message, true, state->summary.channel_name, false);
		}
		return true;
	}
	void Driver::OnSetUserChannelKeys(ChannelSummary summary, UserSummary user_summary, OS::KVReader keys) {
		SetChannelKeysIteratorState state;
		state.summary = summary;
		state.user_summary = user_summary;
		state.keys = keys;

		std::ostringstream ss;
		ss << summary.channel_name << " " << user_summary.nick << " BCAST :" << keys.ToString();
		state.keys_message = ss.str();

		mp_mutex->lock();
		OS::LinkedListIterator<INetPeer*, SetChannelKeysIteratorState*> iterator(mp_peers);
    	iterator.Iterate(LLIterator_OnSetUserChannelKeys, &state);
		mp_mutex->unlock();
	}

	bool Driver::LLIterator_OnSetChannelKeys(INetPeer* peer, SetChannelKeysIteratorState* state) {
		Peer *p = (Peer *)peer;
		if(p->GetChannelFlags(state->summary.channel_id) & EUserChannelFlag_IsInChannel) {
			p->send_numeric(704,state->keys_message, true, state->summary.channel_name, false);
		}
	}
	
	void Driver::OnSetChannelKeys(ChannelSummary summary, OS::KVReader keys) {
		SetChannelKeysIteratorState state;
		state.summary = summary;
		state.keys = keys;

		std::ostringstream ss;
		ss << summary.channel_name << " BCAST :" << keys.ToString();
		state.keys_message = ss.str();

		mp_mutex->lock();
		OS::LinkedListIterator<INetPeer*, SetChannelKeysIteratorState*> iterator(mp_peers);
    	iterator.Iterate(LLIterator_OnSetChannelKeys, &state);
		mp_mutex->unlock();
	}

	bool Driver::LLIterator_OnChannelBroadcast(INetPeer* peer, OnChannelBroadcastState* state) {
		Peer *p = (Peer *)peer;
		if (state->includeSelf && p->GetBackendId() == state->target.id) {
			p->send_message(state->type, state->message, state->target.ToString());
			return true;
		}
		std::map<int, int>::iterator it2 = state->channel_list.begin();
		
		while(it2 != state->channel_list.end()) {
			std::pair<int, int> pair = *it2;
			bool shouldSend = ~pair.second & EUserChannelFlag_Invisible || p->GetOperFlags() & OPERPRIVS_INVISIBLE;
			if(p->GetChannelFlags(pair.first) & EUserChannelFlag_IsInChannel && shouldSend) {
				p->send_message(state->type, state->message,state->target);
				p->RemoveUserAddressVisibility_ByUser(state->target.id);
				return true;
			}
			it2++;
		}
		return true;
	}
	void Driver::OnChannelBroadcast(std::string type, UserSummary target, std::map<int, int> channel_list, std::string message, bool includeSelf) {
		OnChannelBroadcastState state;
		state.type = type;
		state.target = target;
		state.channel_list = channel_list;
		state.message = message;
		state.includeSelf = includeSelf;

		mp_mutex->lock();		
		OS::LinkedListIterator<INetPeer*, OnChannelBroadcastState*> iterator(mp_peers);
    	iterator.Iterate(LLIterator_OnChannelBroadcast, &state);
		mp_mutex->unlock();
	}
	bool Driver::LLIterator_OnSetUserChanModeFlags(INetPeer* peer, OnSetUserChanModeFlagsState* state) {
		Peer *p = (Peer *)peer;
		if(p->GetBackendId() == state->user_id) {
			p->SetChannelFlags(state->channel_id, state->modeflags);
		} else {
			p->OnSetExternalUserChanModeFlags(state->user_id, state->channel_id, state->modeflags);
		}
		return true;
	}
	void Driver::OnSetUserChanModeFlags(int user_id, int channel_id, int modeflags) {
		OnSetUserChanModeFlagsState state;
		state.user_id = user_id;
		state.channel_id = channel_id;
		state.modeflags = modeflags;

		mp_mutex->lock();		
		OS::LinkedListIterator<INetPeer*, OnSetUserChanModeFlagsState*> iterator(mp_peers);
    	iterator.Iterate(LLIterator_OnSetUserChanModeFlags, &state);
		mp_mutex->unlock();

	}
}