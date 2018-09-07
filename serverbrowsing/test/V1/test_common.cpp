#include "common.h"
#include <sstream>


#include <OS/gamespy/enctypex_decoder.h>
#include <OS/gamespy/gsmsalg.h>
#include <OS/gamespy/enctype_shared.h>

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
	state->sent_validation_response = false;

	m_test_connections[source_address] = state;

	return state;
}
void Send_ConnectionInitPacket(SBTestState *test_state) {
	OS::Buffer buff;

	char realvalidate[16];
	test_state->from_gamename = TEST_GAMENAME;
	test_state->from_secretkey = TEST_SECRETKEY;

	gsseckey((unsigned char *)&realvalidate, (unsigned char *)test_state->server_challenge.c_str(), (const unsigned char *)test_state->from_secretkey.c_str(), 0);

	test_state->validation = realvalidate;
	test_state->sent_validation_response = true;
	
    std::ostringstream ss;

    ss << "\\gamename\\" << test_state->from_gamename << "\\gamever\\1.0\\location\\0\\validate\\" << test_state->validation << "\\final\\\\queryid\\1.1\\";
    buff.WriteBuffer(ss.str().c_str(), ss.str().length());

	buff.resetReadCursor();
	test_netio_iface->fake_PushIncomingPacket(test_state->peer->GetSocket(), buff);
	test_state->peer->think(true);
}
void Send_FakeServerGroupsReq(SBTestState *test_state, std::string gamename) {
	OS::Buffer buff;
	
    std::ostringstream ss;

    ss << "\\list\\groups\\gamename\\" << gamename << "\\final\\";

    buff.WriteBuffer(ss.str().c_str(), ss.str().length());

	buff.resetReadCursor();
	test_netio_iface->fake_PushIncomingPacket(test_state->peer->GetSocket(), buff);

	test_state->parse_state = EParseState_ReadFields;
	test_state->peer->think(true);
}
