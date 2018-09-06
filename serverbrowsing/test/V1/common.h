#ifndef _TEST_COMMON
#define _TEST_COMMON
#include <stdio.h>
#include <sstream>
#include <OS/Net/NetServer.h>

#include <OS/test/TestNetIOInterface.h>

#include "server/SBPeer.h"
#include "server/V1Peer.h"

#define TEST_GAMENAME "gmtest"
#define TEST_SECRETKEY "HA6zkS"

extern INetServer *test_gameserver;
extern Test::TestNetIOInterface<> *test_netio_iface;

enum ETestType {
	ETestType_ListReq,
	ETestType_InfoReq,
	ETestType_SendMsgReq,
};
enum EParseState {
	EParseState_ReadFields,
	EParseState_ReadList,
};

typedef struct {
	INetIOSocket *socket;
	ETestType test_type;
	INetPeer *peer;
	INetDriver *driver;
	OS::Address address;

    std::string from_gamename;
	std::string from_secretkey;

    std::string validation;
	std::string server_challenge;


	bool sent_validation_response;

	EParseState parse_state;

	std::vector<std::string> fields;
} SBTestState;

extern std::map<OS::Address, SBTestState *> m_test_connections;

SBTestState *findTestStateByAddress(OS::Address address);
void MapTestConnections(INetDriver *driver);
SBTestState *Make_FakeConnection(INetDriver *driver, OS::Address source_address);


void Send_ConnectionInitPacket(SBTestState *test_state);
void Send_FakeServerGroupsReq(SBTestState *test_state, std::string gamename);

#endif //_TEST_COMMON