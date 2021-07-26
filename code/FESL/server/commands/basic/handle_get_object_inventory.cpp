#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>


#include <sstream>
namespace FESL {
	void Peer::m_get_object_inventory_callback(TaskShared::WebErrorDetails error_details, std::vector<ObjectInventoryItem> results, INetPeer *peer, void *extra) {
		if(error_details.response_code == TaskShared::WebErrorCode_Success) {
			std::ostringstream s;

			s << "TXN=GetObjectInventory\n";
			if((int)extra != -1) {
				s << "TID=" << (int)extra << "\n";
			}

			s << "entitlements.[]=" << results.size() << "\n";

			int idx = 0;
			std::vector<ObjectInventoryItem>::iterator it = results.begin();
			while(it != results.end()) {
				ObjectInventoryItem item = *it;
				s << "entitlements." << idx << "editionNo=" << item.EditionNo << "\n";
				s << "entitlements." << idx << "objectId=" << item.ObjectId << "\n";
	

				char timeBuff[128];
				struct tm *newtime;
				newtime = localtime((time_t *)&item.DateEntitled);

				strftime(timeBuff, sizeof(timeBuff), FESL_DATE_FORMAT, newtime);
				s << "entitlements." << idx << "dateEntitled=" << OS::url_encode(timeBuff) << "\n";
				
				
				s << "entitlements." << idx << "useCount=" << item.UseCount << "\n";
				s << "entitlements." << idx << "entitleId=" << item.EntitleId << "\n";
				idx++;
				it++;
			}

			((Peer *)peer)->SendPacket(FESL_TYPE_DOBJ, s.str());			
		} else {
			((Peer *)peer)->handle_web_error(error_details, FESL_TYPE_DOBJ, "GetObjectInventory", (int)extra);
		}

	}
	bool Peer::m_dobj_get_object_inventory(OS::KVReader kv_list) {

		if (kv_list.HasKey("objectIds.[]")) {

			PublicInfo server_info = ((FESL::Driver *)GetDriver())->GetServerInfo();

			server_info.domainPartition = kv_list.GetValue("domainId");
			server_info.subDomain = kv_list.GetValue("subdomainId");
			server_info.messagingHostname = kv_list.GetValue("partitionKey");
			
			FESLRequest request;
			request.type = EFESLRequestType_GetObjectInventory;
			request.peer = this;
			request.profileid = m_profile.id;
			request.driverInfo = server_info;
			this->IncRef();
			
			int num_objectids = kv_list.GetValueInt("objectIds.[]");
			for(int i=0;i<num_objectids;i++) {
				std::ostringstream s;
				s << "objectIds." << i;
				std::string objectId = kv_list.GetValue(s.str());
				request.objectIds.push_back(objectId);
			}

			request.objectInventoryItemsCallback = m_get_object_inventory_callback;

			TaskScheduler<FESLRequest, TaskThreadData> *scheduler = ((FESL::Server *)(GetDriver()->getServer()))->GetFESLTasks();
			scheduler->AddRequest(request.type, request);
		}
		return true;
	}
}