#include "common.h"
#include <OS/gamespy/enctypex_decoder.h>
INetServer *test_gameserver = NULL;
Test::TestNetIOInterface<> *test_netio_iface = NULL;

std::map<OS::Address, QR2TestState *> m_test_connections;

QR2TestState *findTestStateByAddress(OS::Address address) {
	return m_test_connections[address];
}

void MapTestConnections(INetDriver *driver) {
	std::vector<INetPeer *> peers = driver->getPeers(true);
	std::vector<INetPeer *>::iterator it = peers.begin();
	QR2TestState *state;
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
QR2TestState *Make_FakeConnection(INetDriver *driver, OS::Address source_address, int version) {
	QR2TestState *state = (QR2TestState *)malloc(sizeof(QR2TestState));
	INetIOSocket *socket = new INetIOSocket;
	Test::TestNetIOInterface<> *net_iface = (Test::TestNetIOInterface<> *)driver->getServer()->getNetIOInterface();

	socket->shared_socket = false;
	socket->sd = 0;
	socket->address = source_address;

	state->driver = driver;
	state->peer = ((QR::Driver *)driver)->find_or_create(source_address,driver->getListenerSocket(), version);
	state->socket = socket;
	state->address = source_address;
	state->instance_key = 11;

	m_test_connections[source_address] = state;

	return state;
}


void Handle_QR2Challenge(QR2TestState *state, OS::Buffer buffer) {
	uint16_t magic;
	uint32_t instance_key;

	buffer.resetReadCursor();
	buffer.ReadByte();
	magic = buffer.ReadShort();
	instance_key = buffer.ReadInt();

	std::string challenge = buffer.ReadNTS();

	char challenge_response[90] = { 0 };
	gsseckey((unsigned char *)&challenge_response, (unsigned char *)challenge.c_str(), TEST_SECRETKEY, 0);

	INetIODatagram dgram;
	dgram.buffer.WriteByte(PACKET_CHALLENGE);
	dgram.buffer.WriteInt(state->instance_key);
	dgram.buffer.WriteNTS(challenge_response);

    
    dgram.address = state->address;
    dgram.comm_len = dgram.buffer.bytesWritten();
	dgram.buffer.resetReadCursor();

	state->state = ETestState_WaitRegistered;

    test_netio_iface->fake_PushIncomingDatagram(state->driver->getListenerSocket(), dgram);
	state->driver->think(true);
}
void Handle_QR2Registered(QR2TestState *state, OS::Buffer buffer) {
	uint16_t magic;
	uint32_t instance_key;

	buffer.resetReadCursor();
	magic = buffer.ReadShort();
	instance_key = buffer.ReadInt();

	printf("QR2 registered\n");
}