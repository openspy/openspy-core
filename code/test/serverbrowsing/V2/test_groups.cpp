#include "common.h"

#include <OS/Redis.h>

#include "server/SBServer.h"
#include "server/SBDriver.h"
#include "server/V2Peer.h"

#define FAKE_PUBLIC_IP 0xAABBCCDD

#define LOOKUP_GAMENAME "thugpro"

bool Fake_SendCallback(INetIOSocket *socket, OS::Buffer buffer) {
	buffer.resetReadCursor();
    int len = buffer.readRemaining();

	static bool parsed_list_resp = false;

	if(parsed_list_resp) return false;

    SBTestState *state = findTestStateByAddress(socket->address);
    int header_len = 0;
    if(!state->setup_cryptstate) {
        header_len = SB2_InitCryptState(state, buffer);
        state->setup_cryptstate = true;
    }

    GOADecrypt(&state->crypt_state, ((unsigned char *)buffer.GetHead()) + header_len, len - header_len);
	
    ServerListResponse resp = SB2_ParseServerListResp(state, buffer);

	if(resp.public_ip.GetIP() != FAKE_PUBLIC_IP) exit(-1);

	if(resp.entries.size() != 2) exit(-2);

	if(resp.entries[0].field_values["hostname"].compare("Beenox") != 0) exit(-3);

	exit(0);

	parsed_list_resp = true;

	return false;
}


int main() {
	OS::Init("serverbrowsing", "openspy.cfg");

	test_gameserver = new SBServer();
	test_gameserver->init();

	test_netio_iface = new Test::TestNetIOInterface<>();
	test_netio_iface->fake_SetSendCallback(Fake_SendCallback);

	test_gameserver->setNetIOInterface(test_netio_iface);
	
    SB::Driver *driver = new SB::Driver(test_gameserver, "127.0.0.1", 28910, 2);

	test_gameserver->addNetworkDriver(driver);

	OS::Sleep(1000);

    OS::Address address;
    address.ip = FAKE_PUBLIC_IP;
    address.port = rand();

    Make_FakeConnection(driver, address);

	driver->think(true); //trigger TCP accept
	MapTestConnections(driver);

	std::vector<INetPeer *> peers = driver->getPeers();
	std::map<OS::Address, SBTestState *>::iterator it = m_test_connections.begin();
	while (it != m_test_connections.end()) {
		SBTestState *state = (*it).second;
		Send_FakeServerGroupsReq(state, LOOKUP_GAMENAME);
		it++;
	}
	
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

    return -1;
}