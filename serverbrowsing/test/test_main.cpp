#include <stdio.h>
#include <OS\test\TestDriver.h>
#include <OS\Net\NetServer.h>

#include <OS\test\TestNetIOInterface.h>
#include "server\SBServer.h"
#include "server\SBDriver.h"
INetServer *test_gameserver = NULL;
Test::TestNetIOInterface<> *test_netio_iface = NULL;

enum ETestType {
	ETestType_ListReq,
	ETestType_InfoReq,
	ETestType_SendMsgReq,
};
typedef struct {
	INetIOSocket *socket;
	ETestType test_type;
	INetPeer *peer;
	INetDriver *driver;
	OS::Address address;
} SBTestState;

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

	m_test_connections[source_address] = state;

	return state;
}

void Send_FakeServerListReq(SBTestState *test_state, std::string gamename) {

	OS::Buffer buff;
	buff.WriteShort(htons(777)); //length
	buff.WriteByte(SERVER_LIST_REQUEST); //request type
	buff.WriteByte(6); //protocol version
	buff.WriteByte(7); //encoding version
	buff.WriteInt(8); //game version
	buff.WriteNTS(gamename);
	buff.WriteNTS(gamename);
	buff.WriteNTS(""); //filter
 	buff.WriteNTS(""); //key list
	buff.WriteInt(htonl(SEND_GROUPS)); //options
	buff.resetReadCursor();
	test_netio_iface->fake_PushIncomingPacket(test_state->peer->GetSocket(), buff);
	test_state->peer->think(true);
}
void Send_FakeServerMsgReq(SBTestState *test_state, OS::Address dest, char *buf, int buff_len) {
	OS::Buffer buff;
	buff.WriteShort(htons(777)); //length
	buff.WriteByte(SEND_MESSAGE_REQUEST); //request type
	buff.WriteInt(dest.GetIP());
	buff.WriteShort(dest.GetPort());
	buff.WriteBuffer(buf, buff_len);
	buff.resetReadCursor();
	test_netio_iface->fake_PushIncomingPacket(test_state->peer->GetSocket(), buff);
	test_state->peer->think(true);
}
bool Fake_SendCallback(INetIOSocket *socket, OS::Buffer buffer) {
	printf("Got fake data: %d\n", buffer.bytesWritten());
	return false;
}

int test_main() {
	OS::Init("serverbrowsing", "openspy.cfg");

	test_gameserver = new SBServer();
	test_gameserver->init();

	test_netio_iface = new Test::TestNetIOInterface<>();
	test_netio_iface->fake_SetSendCallback(Fake_SendCallback);

	test_gameserver->setNetIOInterface(test_netio_iface);
	
    SB::Driver *driver = new SB::Driver(test_gameserver, "127.0.0.1", 28910, 2);


	for (int i = 0; i < 17500; i++) {
		OS::Address address;
		address.ip = rand();
		address.port = rand();
		Make_FakeConnection(driver, address);
	}


	driver->think(true); //trigger TCP accept
	MapTestConnections(driver);


	//std::vector<SBTestState *> m_test_connections
	std::vector<INetPeer *> peers = driver->getPeers();
	std::map<OS::Address, SBTestState *>::iterator it = m_test_connections.begin();
	while (it != m_test_connections.end()) {
		SBTestState *state = (*it).second;
		Send_FakeServerListReq(state, "thugpro");
		Send_FakeServerMsgReq(state, OS::Address("55.11.22.11:666"), "test", 4);
		it++;
	}
	

	/*
	std::vector<INetPeer *> peers = driver->getPeers(true);
	INetPeer *peer = peers.front();

	Send_FakeServerListReq(peer, "thugpro");*/

	OS::Sleep(75000);

	it = m_test_connections.begin();
	while (it != m_test_connections.end()) {
		std::pair<OS::Address, SBTestState *> entry = *it;

		Test::TestNetIOInterface<> *net_iface = (Test::TestNetIOInterface<> *)entry.second->driver->getServer()->getNetIOInterface();

		net_iface->fake_TCPDisconnect(entry.second->socket);
		entry.second->peer->DecRef();
		entry.second->peer->think(true);
		it++;
	}

    return 0;
}