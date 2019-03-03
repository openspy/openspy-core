#ifndef OS_TASKSHARED_GEOGRAPHY_H
#define OS_TASKSHARED_GEOGRAPHY_H
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
#include <OS/OpenSpy.h>
#include <OS/Profile.h>
#include <OS/User.h>
#include <curl/curl.h>
#include <jansson.h>
#include <string>
#include <OS/Cache/GameCache.h>

#define REGIONID_AMERICAS 1
#define REGIONID_NORTH_AMERICA 2
#define REGIONID_CARIBBEAN 4
#define REGIONID_CENTRAL_AMERICA 8
#define REGIONID_SOUTH_AMERICA 16
#define REGIONID_AFRICA 32
#define REGIONID_CENTRAL_AFRICA 64
#define REGIONID_EAST_AFRICA 128
#define REGIONID_NORTH_AFRICA 256
#define REGIONID_SOUTH_AFRICA 512
#define REGIONID_WEST_AFRICA 1024
#define REGIONID_ASIA 2048
#define REGIONID_EAST_ASIA 4096
#define REGIONID_PACIFIC 8192
#define REGIONID_SOUTH_ASIA 16384
#define REGIONID_SOUTH_EAST_ASIA 32768
#define REGIONID_EUROPE 65536
#define REGIONID_BALTIC_STATES 131072
#define REGIONID_CIS 262144
#define REGIONID_EASTERN_EUROPE 524288
#define REGIONID_MIDDLE_EAST 1048576
#define REGIONID_SOUTH_EAST_EUROPE 2097152
#define REGIONID_WESTERN_EUROPE 4194304


namespace TaskShared {
	class CountryRegion {
		public:
		std::string countrycode;
		std::string countryname;
		uint32_t region;
	};

     enum EGeoTaskType {
        EGeoTaskType_GetCountries,
    };

	typedef struct {
		std::vector<CountryRegion> countries;
		WebErrorDetails error_details;
	} GeoTaskData;

    typedef void (*GeographyCallback)(GeoTaskData auth_data, void *extra, INetPeer *peer);

	class GeoRequest {
		public:
			GeoRequest() {
				extra = NULL;
				peer = NULL;
				callback = NULL;
			}
			int type;

			GeographyCallback callback;
			void *extra;

			INetPeer *peer;
	};

	void GeoReq_InitCurl(void *curl, char *post_data, void *write_data, GeoRequest request);
    bool PerformGeo_GetCountries(GeoRequest request, TaskThreadData *thread_data);
}
#endif //OS_TASKSHARED_GEOGRAPHY_H
