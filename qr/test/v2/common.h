#ifndef _TEST_COMMON
#define _TEST_COMMON
#include <stdio.h>
#include <sstream>
#include <OS/Net/NetServer.h>

#include <OS/test/TestNetIOInterface.h>

#include "server/V2Peer.h"

#define TEST_GAMENAME "gmtest"
#define TEST_SECRETKEY "HA6zkS"

extern INetServer *test_gameserver;
extern Test::TestNetIOInterface<> *test_netio_iface;

enum ETestType {
	ETestType_ServerRegister, //this server push must pass
	ETestType_ServerRegister_Fail, //this server push must fail
};
enum ETestState {
	ETestState_WaitChallenge,
	ETestState_WaitRegistered
};

typedef struct _QR2TestState {
	INetIOSocket *socket;
	INetPeer *peer;
	INetDriver *driver;
	OS::Address address;
	uint32_t instance_key;

	ETestState state;
	ETestType test_type;

	int identifier;

	bool (*challenge_handler)(struct _QR2TestState *state, OS::Buffer buffer);
	bool (*registered_handler)(struct _QR2TestState *state, OS::Buffer buffer);
} QR2TestState;

extern std::map<OS::Address, QR2TestState *> m_test_connections;

QR2TestState *findTestStateByAddress(OS::Address address);
void MapTestConnections(INetDriver *driver);
QR2TestState *Make_FakeConnection(INetDriver *driver, OS::Address source_address, int version = 2);

void Handle_QR2Challenge(QR2TestState *state, OS::Buffer buffer);
void Handle_QR2Registered(QR2TestState *state, OS::Buffer buffer);
#endif //_TEST_COMMON