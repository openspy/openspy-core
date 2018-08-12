#include <stdio.h>
#include "common.h"
#include <OS/Net/NetServer.h>

#include <OS/test/TestNetIOInterface.h>
#include "server/SBServer.h"
#include "server/SBDriver.h"
INetServer *test_gameserver = NULL;
Test::TestNetIOInterface<> *test_netio_iface = NULL;

std::map<OS::Address, SBTestState *> m_test_connections;

SBTestState *findTestStateByAddress(OS::Address address) {
	return m_test_connections[address];
}

void MapTestConnections(INetDriver *driver) {
	std::vector<INetPeer *> peers = driver->getPeers(true);
	std::vector<INetPeer *>::iterator it = peers.begin();
	SBTestState *state;
	INetPeer *peer;
	while (it != peers.end()) {
		peer = *it;
		state = findTestStateByAddress(peer->getAddress());
		if (state != NULL && state->peer == NULL) {
			state->peer = peer;
		}
		else {
			peer->DecRef();
		}
		it++;
	}
	
}
SBTestState *Make_FakeConnection(INetDriver *driver, OS::Address source_address) {
	SBTestState *state = (SBTestState *)malloc(sizeof(SBTestState));
	INetIOSocket *socket = new INetIOSocket;
	Test::TestNetIOInterface<> *net_iface = (Test::TestNetIOInterface<> *)driver->getServer()->getNetIOInterface();

	socket->shared_socket = false;
	socket->sd = 0;
	socket->address = source_address;
	net_iface->fake_AddTCPConnection(socket);

	state->driver = driver;
	state->peer = NULL;
	state->socket = socket;
	state->address = source_address;
	state->setup_cryptstate = false;
	memset(&state->crypt_state,0,sizeof(state->crypt_state));

	m_test_connections[source_address] = state;

	return state;
}


void Send_FakeServerNoServerReq(SBTestState *test_state, std::string gamename) {
	OS::Buffer buff;
	buff.WriteShort(htons(777)); //length
	buff.WriteByte(SERVER_LIST_REQUEST); //request type
	buff.WriteByte(6); //protocol version
	buff.WriteByte(7); //encoding version
	buff.WriteInt(8); //game version
	buff.WriteNTS(gamename);
	buff.WriteNTS(TEST_GAMENAME);
	buff.WriteBuffer(SB_V2_CHALLENGE, LIST_CHALLENGE_LEN);
	buff.WriteNTS(""); //filter
 	buff.WriteNTS(""); //key list
	buff.WriteInt(htonl(NO_SERVER_LIST)); //options
	buff.resetReadCursor();
	test_netio_iface->fake_PushIncomingPacket(test_state->peer->GetSocket(), buff);
	test_state->peer->think(true);
}

void Send_FakeServerGroupsReq(SBTestState *test_state, std::string gamename) {
	OS::Buffer buff;
	buff.WriteShort(htons(777)); //length
	buff.WriteByte(SERVER_LIST_REQUEST); //request type
	buff.WriteByte(6); //protocol version
	buff.WriteByte(7); //encoding version
	buff.WriteInt(8); //game version
	buff.WriteNTS(gamename);
	buff.WriteNTS(TEST_GAMENAME);
	buff.WriteBuffer(SB_V2_CHALLENGE, LIST_CHALLENGE_LEN);
	buff.WriteNTS(""); //filter
 	buff.WriteNTS("\\hostname"); //key list
	buff.WriteInt(htonl(SEND_GROUPS)); //options
	buff.resetReadCursor();
	test_netio_iface->fake_PushIncomingPacket(test_state->peer->GetSocket(), buff);
	test_state->peer->think(true);
}

int SB2_InitCryptState(SBTestState *state, OS::Buffer &buffer) {
	const int seckeylen = strlen(TEST_SECRETKEY);
	char *seckey = TEST_SECRETKEY;

    buffer.resetReadCursor();
    
    uint8_t cryptlen = buffer.ReadByte() ^ 0xEC;
    uint8_t cryptchal[CRYPTCHAL_LEN];
    memset(&cryptchal, 0, sizeof(cryptchal));

    if(cryptlen > sizeof(cryptchal)) {
        return false;
    }
    buffer.ReadBuffer(&cryptchal, cryptlen);

    uint8_t servchallen = buffer.ReadByte() ^ 0xEA;
    uint8_t servchal[SERVCHAL_LEN];
    memset(&servchal, 0, sizeof(servchal));

    if(servchallen > sizeof(servchal)) {
        return false;
    }

    buffer.ReadBuffer(&servchal, servchallen);
   
    memcpy(state->challenge, SB_V2_CHALLENGE, LIST_CHALLENGE_LEN);

    for (uint32_t i = 0 ; i < servchallen ; i++)
    {
        state->challenge[(i *  seckey[i % seckeylen]) % LIST_CHALLENGE_LEN] ^= (char)((state->challenge[i % LIST_CHALLENGE_LEN] ^ servchal[i]) & 0xFF);
    }

    GOACryptInit(&(state->crypt_state), (unsigned char *)(state->challenge), LIST_CHALLENGE_LEN);

	return servchallen + cryptlen + 2;
}

ServerListResponse SB2_ParseServerListResp(SBTestState *state, OS::Buffer &buffer) {
	static bool got_response = false;
	ServerListResponse list_response;

	if(!got_response) {
		got_response = true;
	} else {
		return list_response;
	}

    uint32_t ip = buffer.ReadInt();
    list_response.public_ip =  OS::Address(ip, 0);
    list_response.query_port = buffer.ReadShort();

    uint8_t field_size = buffer.ReadByte();
	std::map<std::string, uint8_t> fields;
    for(int i=0;i<field_size;i++) {
        uint8_t type = buffer.ReadByte();
        std::string field_name = buffer.ReadNTS();
		fields[field_name] = type;
    }

	list_response.fields = fields;
    //would read popular values
	uint8_t num_popular_values = buffer.ReadByte();
	std::string popular_value;
	for(int i=0;i<num_popular_values;i++) {
		popular_value = buffer.ReadNTS();
		list_response.popular_values.push_back(popular_value);
	}

	while(SB2_ParseServerEntry(state, buffer, list_response));

	return list_response;
}
bool SB2_ParseServerEntry(SBTestState *state, OS::Buffer &buffer, ServerListResponse &list_response) {
	ServerEntry entry;
    uint8_t flags;
    uint32_t server_ip;

	flags = buffer.ReadByte();
	server_ip = buffer.ReadInt();

	if(server_ip == 0xFFFFFFFF)
		return false;

	if (flags & NONSTANDARD_PORT_FLAG) {
		buffer.ReadShort();
	}

	if (flags & PRIVATE_IP_FLAG) {
		buffer.ReadInt();
	}

	if (flags & NONSTANDARD_PRIVATE_PORT_FLAG) {
		buffer.ReadShort();
	}

	entry.address = OS::Address(server_ip, list_response.query_port);

	if(flags & HAS_KEYS_FLAG) {
		std::string s_val;
		uint8_t b_val;
		uint16_t sh_val;

		std::ostringstream ss;

		std::map<std::string, uint8_t>::iterator fields_it = list_response.fields.begin();

		while(fields_it != list_response.fields.end()) {
			std::pair<std::string, uint8_t> p = *fields_it;
			uint8_t index = buffer.ReadByte();
			if(index == 0xFF) {
				switch(p.second) {
					case KEYTYPE_BYTE:
						b_val = buffer.ReadByte();
						ss << b_val;
						entry.field_values[p.first] = ss.str();
						ss.str("");
					break;
					case KEYTYPE_SHORT:
						sh_val = buffer.ReadShort();
						ss << sh_val;
						entry.field_values[p.first] = ss.str();
						ss.str("");
					break;
					case KEYTYPE_STRING:
						s_val = buffer.ReadNTS();
						entry.field_values[p.first] = s_val;
					break;
				}
			}
			fields_it++;
		}
	}
	if(flags & HAS_FULL_RULES_FLAG) {
		while(true) {
			std::string key, value;
			key = buffer.ReadNTS();
			if(key.length() == 0) break;
			value = buffer.ReadNTS();
			entry.field_values[key] = value;
		}
	}

	printf("server ip: %08x\n", server_ip);
	list_response.entries.push_back(entry);
	return true;
}