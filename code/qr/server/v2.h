#ifndef _V2_CONSTANTS
#define _V2_CONSTANTS

	#define REQUEST_KEY_LEN 4
	#define QR2_PING_TIME 120

	#define QR2_RESEND_MSG_TIME 5 //resend every 5 seconds
	#define QR2_MAX_RESEND_COUNT 5

	#define PACKET_QUERY              0x00  //S -> C
	#define PACKET_CHALLENGE          0x01  //S -> C
	#define PACKET_ECHO               0x02  //S -> C (purpose..?)
	#define PACKET_ECHO_RESPONSE      0x05  // 0x05, not 0x03 (order) | C -> S
	#define PACKET_HEARTBEAT          0x03  //C -> S
	#define PACKET_ADDERROR           0x04  //S -> C
	#define PACKET_CLIENT_MESSAGE     0x06  //S -> C
	#define PACKET_CLIENT_MESSAGE_ACK 0x07  //C -> S
	#define PACKET_KEEPALIVE          0x08  //S -> C | C -> S
	#define PACKET_PREQUERY_IP_VERIFY 0x09  //S -> C
	#define PACKET_AVAILABLE          0x09  //C -> S
	#define PACKET_CLIENT_REGISTERED  0x0A  //S -> C

	#define QR2_OPTION_USE_QUERY_CHALLENGE 128 //backend flag

	#define QR_MAGIC_1 0xFE
	#define QR_MAGIC_2 0xFD

	#define V2_CHALLENGE_LEN 20

#endif //_V2_CONSTANTS