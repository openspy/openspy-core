#ifndef _TEST_COMMON
#define _TEST_COMMON
#include <stdio.h>
#include <sstream>
#include <OS/Net/NetServer.h>

#include <OS/test/TestNetIOInterface.h>

#include "server/V2Peer.h"
#include "server/sb_crypt.h"

#define TEST_GAMENAME "gmtest"
#define TEST_SECRETKEY "HA6zkS"
#define SB_V2_CHALLENGE "test1234"

extern INetServer *test_gameserver;
extern Test::TestNetIOInterface<> *test_netio_iface;

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

	GOACryptState crypt_state;
	char challenge[LIST_CHALLENGE_LEN];
	bool setup_cryptstate;
} SBTestState;


typedef struct {
	OS::Address address;
	OS::Address private_ip;
	std::map<std::string, std::string> field_values;
} ServerEntry;

typedef struct {
	OS::Address public_ip;
	uint16_t query_port;

	std::map<std::string, uint8_t> fields;
	std::vector<std::string> popular_values;
	std::vector<ServerEntry> entries;
} ServerListResponse;

extern std::map<OS::Address, SBTestState *> m_test_connections;

SBTestState *findTestStateByAddress(OS::Address address);
void MapTestConnections(INetDriver *driver);
SBTestState *Make_FakeConnection(INetDriver *driver, OS::Address source_address);


void Send_FakeServerNoServerReq(SBTestState *test_state, std::string gamename);
void Send_FakeServerGroupsReq(SBTestState *test_state, std::string gamename);

int SB2_InitCryptState(SBTestState *test_state, OS::Buffer &buffer);
ServerListResponse SB2_ParseServerListResp(SBTestState *state, OS::Buffer &buffer);

bool SB2_ParseServerEntry(SBTestState *state, OS::Buffer &buffer, ServerListResponse &list_response);
#endif //_TEST_COMMON