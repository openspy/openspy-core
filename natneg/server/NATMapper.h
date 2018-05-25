#ifndef _NAT_MAPPER_H
#define _NAT_MAPPER_H
#include <OS/OpenSpy.h>
#include "structs.h"

namespace NN {

	bool DetermineNatType(NAT &nat);
	void DetermineNextAddress(NAT &nat, OS::Address &next_public_address, OS::Address &next_private_address);
	void LoadSummaryIntoNAT(NN::ConnectionSummary summary, NAT &nat);
}
#endif //_NAT_MAPPER_H