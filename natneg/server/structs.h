#ifndef _NN_STRUCTS_INC
#define _NN_STRUCTS_INC

namespace NN {

#define NATNEG_MAGIC_LEN 6
#define NN_MAGIC_0 0xFD
#define NN_MAGIC_1 0xFC
#define NN_MAGIC_2 0x1E
#define NN_MAGIC_3 0x66
#define NN_MAGIC_4 0x6A
#define NN_MAGIC_5 0xB2

#define MATCHUP_PORT1 27901
#define MATCHUP_PORT2 27901
#define MATCHUP_PORT3 27901
#define MATCHUP_PORT MATCHUP_PORT1

#define FINISHED_NOERROR 0
#define FINISHED_ERROR_DEADBEAT_PARTNER 1
#define FINISHED_ERROR_INIT_PACKETS_TIMEDOUT 2

#define PREINIT_RETRY_TIME 500
#define PREINIT_RETRY_COUNT 10
#define PREINITACK_RETRY_TIME 10000
#define PREINITACK_RETRY_COUNT 12
#define INIT_RETRY_TIME 500
#define INIT_RETRY_COUNT 10
#define NNINBUF_LEN 512
#define PING_RETRY_TIME 700
#define PING_RETRY_COUNT 7
#define PARTNER_WAIT_TIME 60000
#define REPORT_RETRY_TIME 500
#define REPORT_RETRY_COUNT 4

#define NN_PROTVER 4
	//#define NN_PROTVER 3
	//#define NN_PROTVER 2

#define NN_PT_GP  0
#define NN_PT_NN1 1
#define NN_PT_NN2 2
#define NN_PT_NN3 3

#define NN_INIT	0
#define NN_INITACK 1
#define NN_ERTTEST 2
#define NN_ERTACK 3
#define NN_STATEUPDATE 4
#define NN_CONNECT 5
#define NN_CONNECT_ACK 6
#define NN_CONNECT_PING 7
#define NN_BACKUP_TEST 8
#define NN_BACKUP_ACK 9
#define NN_ADDRESS_CHECK 10
#define NN_ADDRESS_REPLY 11
#define NN_NATIFY_REQUEST 12
#define NN_REPORT 13
#define NN_REPORT_ACK 14
#define NN_PREINIT 15
#define NN_PREINIT_ACK 16

#define NN_PREINIT_WAITING_FOR_CLIENT 0
#define NN_PREINIT_WAITING_FOR_MATCHUP 1
#define NN_PREINIT_READY 2

	typedef enum { packet_map1a, packet_map2, packet_map3, packet_map1b, NUM_PACKETS } NatifyPacket;
	typedef enum { no_nat, firewall_only, full_cone, restricted_cone, port_restricted_cone, symmetric, unknown, NUM_NAT_TYPES } NatType;
	typedef enum { promiscuous, not_promiscuous, port_promiscuous, ip_promiscuous, promiscuity_not_applicable, NUM_PROMISCUITY_TYPES } NatPromiscuity;
	typedef enum { unrecognized, private_as_public, consistent_port, incremental, mixed, NUM_MAPPING_SCHEMES } NatMappingScheme;
	extern uint8_t NNMagicData[];
#if !defined(_PS2) && !defined(_NITRO)
# pragma pack(1)
#endif

#define INITPACKET_SIZE				(BASEPACKET_SIZE + 9)
#define INITPACKET_ADDRESS_OFFSET	(BASEPACKET_SIZE + 3)
	typedef struct _InitPacket
	{
		uint8_t porttype;
		uint8_t clientindex;
		uint8_t usegameport;
		uint32_t localip;
		uint16_t localport;
	} InitPacket;

#define REPORTPACKET_SIZE			(BASEPACKET_SIZE + 61)
	typedef struct _ReportPacket
	{
		uint8_t porttype;
		uint8_t clientindex;
		uint8_t negResult;
		NatType natType;
		NatMappingScheme natMappingScheme;
		char gamename[50];
	} ReportPacket;

#define CONNECTPACKET_SIZE			(BASEPACKET_SIZE + 8)
	typedef struct _ConnectPacket
	{
		uint32_t remoteIP;
		uint16_t remotePort;
		uint8_t gotyourdata;
		uint8_t finished;
	} ConnectPacket;

#define PREINITPACKET_SIZE			(BASEPACKET_SIZE + 6)
	typedef struct _PreinitPacket
	{
		uint8_t clientindex;
		uint8_t state;
		uint32_t clientID;
	} PreinitPacket;

#define BASEPACKET_SIZE				12
#define BASEPACKET_TYPE_OFFSET		7

	typedef struct _NatNegPacket {
		// Base members:
		uint8_t magic[NATNEG_MAGIC_LEN];
		uint8_t version;
		uint8_t packettype;
		uint32_t cookie;

		union
		{
			InitPacket Init;
			ConnectPacket Connect;
			ReportPacket Report;
			PreinitPacket Preinit;
		} Packet;

	} NatNegPacket;

#if !defined(_PS2) && !defined(_NITRO)
# pragma pack()
#endif


	//////////////////////////////////////////////////////////////
	// AddressMapping
	// Summary
	//		Internal and external address pairing for an observed network address translation.
	// See Also
	//		NAT
	typedef struct _AddressMapping {
		unsigned int privateIp;		// Internal IP address.
		unsigned short privatePort;	// Internal port number.
		unsigned int publicIp;		// External IP address.
		unsigned short publicPort;	// External port number.
	} AddressMapping;


	typedef struct _NAT {
		bool ipRestricted;			// true if the NAT drops packets from unsolicited IP addresses.
		bool portRestricted;		// true if the NAT drops packets from unsolicited ports.
		NatPromiscuity promiscuity;		// The type of promiscuity the NAT allows.
		NatType natType;				// The type of NAT as defined by RFC2663.
		NatMappingScheme mappingScheme;	// The type of port mapping/allocation scheme used by the NAT.
		AddressMapping mappings[4];		// Port mappings observed during the detection process.

		AddressMapping privateAddress; //the private address mapping for next private connection
	} NAT;

	typedef uint32_t NNCookieType;

	class ConnectionSummary {
	public:
		ConnectionSummary() {
			usegameport = 0;
			cookie = 0;
			gameport = 0;
			port_type = 0;
			required_addresses = 0;
			address_hits = 0;
		}
		bool usegameport;
		NNCookieType cookie;
		int index;
		int port_type;
		int address_hits;
		int required_addresses;
		uint16_t gameport;
		std::map<int, OS::Address> m_port_type_addresses;
		OS::Address private_address;
	};
}
#endif
