#include "NATMapper.h"
namespace NN {
	uint8_t NNMagicData[] = { NN_MAGIC_0, NN_MAGIC_1, NN_MAGIC_2, NN_MAGIC_3, NN_MAGIC_4, NN_MAGIC_5 };
	bool DetermineNatType(NAT &nat) {
		bool has_gameport = nat.mappings[packet_map1a].publicPort != 0;
		bool has_nn_serv_3 = nat.mappings[packet_map1b].publicPort != 0;

		// Initialize.
		nat.natType = unknown;
		nat.promiscuity = promiscuity_not_applicable;

		// Is there a NAT?
		if (!nat.portRestricted &&
			!nat.ipRestricted &&
			(nat.mappings[packet_map2].publicIp == nat.mappings[packet_map2].privateIp))
		{
			nat.natType = no_nat;
		}
		else if (nat.mappings[packet_map2].publicIp == nat.mappings[packet_map2].privateIp)
		{
			nat.natType = firewall_only;
		}
		else
		{
			// What type of NAT is it?
			if (!nat.ipRestricted &&
				!nat.portRestricted &&
				(abs(nat.mappings[packet_map3].publicPort - nat.mappings[packet_map2].publicPort) >= 1))
			{
				nat.natType = symmetric;
				nat.promiscuity = promiscuous;
			}
			else if (nat.ipRestricted &&
				!nat.portRestricted &&
				(abs(nat.mappings[packet_map3].publicPort - nat.mappings[packet_map2].publicPort) >= 1))
			{
				nat.natType = symmetric;
				nat.promiscuity = port_promiscuous;
			}
			else if (!nat.ipRestricted &&
				nat.portRestricted &&
				(abs(nat.mappings[packet_map3].publicPort - nat.mappings[packet_map2].publicPort) >= 1))
			{
				nat.natType = symmetric;
				nat.promiscuity = ip_promiscuous;
			}
			else if (nat.ipRestricted &&
				nat.portRestricted &&
				(abs(nat.mappings[packet_map3].publicPort - nat.mappings[packet_map2].publicPort) >= 1))
			{
				nat.natType = symmetric;
				nat.promiscuity = not_promiscuous;
			}
			else if (nat.portRestricted)
				nat.natType = port_restricted_cone;
			else if (nat.ipRestricted && !nat.portRestricted)
				nat.natType = restricted_cone;
			else if (!nat.ipRestricted && !nat.portRestricted)
				nat.natType = full_cone;
			else
				nat.natType = unknown;
		}

		// What is the port mapping behavior?
		if ((!has_gameport || nat.mappings[packet_map1a].publicPort == nat.mappings[packet_map1a].privatePort) &&
			(nat.mappings[packet_map2].publicPort == 0 || nat.mappings[packet_map2].publicPort == nat.mappings[packet_map2].privatePort) &&
			(nat.mappings[packet_map3].publicPort == 0 || nat.mappings[packet_map3].publicPort == nat.mappings[packet_map3].privatePort) &&
			(!has_nn_serv_3 || nat.mappings[packet_map1b].publicPort == nat.mappings[packet_map1b].privatePort)
			) {
			// Using private port as the public port.
			nat.mappingScheme = private_as_public;
		}
		else if (
			nat.mappings[packet_map2].publicPort == nat.mappings[packet_map3].publicPort &&
			(nat.mappings[packet_map1b].publicPort == 0 || nat.mappings[packet_map3].publicPort == nat.mappings[packet_map1b].publicPort)) {
			// Using the same public port for all requests from the same private port.
			nat.mappingScheme = consistent_port;
		}
		else if ((has_gameport && (nat.mappings[packet_map1a].publicPort == nat.mappings[packet_map1a].privatePort)) &&
			nat.mappings[packet_map3].publicPort - nat.mappings[packet_map2].publicPort == 1) {
			// Using private port as the public port for the first mapping.
			// Using an incremental (+1) port mapping scheme there after.
			nat.mappingScheme = mixed;
		}
		else if (nat.mappings[packet_map3].publicPort - nat.mappings[packet_map2].publicPort == 1) {
			// Using an incremental (+1) port mapping scheme.
			nat.mappingScheme = incremental;
		}
		else {
			// Unrecognized port mapping scheme.
			nat.mappingScheme = unrecognized;
		}

		return true;
	}

	void DetermineNextAddress(NAT &nat, OS::Address &next_public_address, OS::Address &next_private_address) {
		OS::Address top_address, bottom_address, address;

		int i = -1;
		if (nat.mappings[0].publicIp != 0) {
			top_address = OS::Address(nat.mappings[0].publicIp, nat.mappings[0].publicPort);
			next_private_address = OS::Address(nat.mappings[0].privateIp, nat.mappings[0].privatePort); //for now just set private address to first connection
		}
		else {
			top_address = OS::Address(nat.mappings[1].publicIp, nat.mappings[1].publicPort); //every natneg protocol sends porttype 1
			next_private_address = OS::Address(nat.mappings[1].privateIp, nat.mappings[1].privatePort); //for now just set private address to first connection
			i++; //account for missing address 0(game socket)
		}

		do { //find last index
			i++;
		} while (i+1 < (int)(sizeof(nat.mappings) / sizeof(AddressMapping)) && nat.mappings[i].publicPort != 0);

		bottom_address = OS::Address(nat.mappings[i].publicIp, nat.mappings[i].publicPort);
		address = bottom_address;

		switch (nat.mappingScheme) {
			case private_as_public: //first connection = game socket
				address = top_address;
				address.port = next_private_address.port;
				break;
			case unrecognized: //good default
			case consistent_port: //since same public port from address, and natneg will send connect packet from first connection.
				address = top_address;
				address.port = top_address.port;
				break;
			case mixed: //unknown
			case incremental: //should work if no new connections were made during natneg process
				address = bottom_address;
				address.port = htons(ntohs(bottom_address.port) + 1);
				break;
			default:
			break;

		}
		next_public_address = address;
	}
	void LoadSummaryIntoNAT(NN::ConnectionSummary summary, NAT &nat) {
		memset(&nat.mappings, 0, sizeof(nat.mappings));
		std::map<int, OS::Address>::iterator it = summary.m_port_type_addresses.begin();

		while (it != summary.m_port_type_addresses.end()) {
			std::pair<int, OS::Address> p = *it;
			if (p.first < (int)sizeof(nat.mappings)) {
				nat.mappings[p.first].privateIp = summary.private_address.GetIP();
				nat.mappings[p.first].privatePort = summary.private_address.GetPort();
				nat.mappings[p.first].publicIp = summary.m_port_type_addresses[p.first].GetIP();
				nat.mappings[p.first].publicPort = summary.m_port_type_addresses[p.first].GetPort();
			}
			it++;
		}

		nat.ipRestricted = false;
		nat.portRestricted = false;
	}

	const char *GetNatMappingSchemeString(NAT nat) {
		switch (nat.mappingScheme) {
		default:
		case unrecognized:
			return "unrecognized";
			break;
		case private_as_public:
			return "private as public";
			break;
		case consistent_port:
			return "consistent Port";
			break;
		case incremental:
			return "incremental";
			break;
		case mixed:
			return "mixed";
			break;
		}
		return "invalid";
	}
}