#ifndef _OS_ANALYTICS_MGR_H
#define _OS_ANALYTICS_MGR_H
#include <jansson.h>
#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Net/NetPeer.h>
#include <OS/Net/NetDriver.h>
#include <OS/Net/NetServer.h>
#include "Metric.h"

#define ANALYTICS_SUBMIT_TIME 15
#define ANALYTICS_SUBMIT_TIME_MS 15000
#define OPENSPY_ANALYTICS_MGR_URL "http://192.168.0.123/stats/submit"

namespace OS {
	typedef struct _AnalyticsRequest {
		MetricInstance instance;
	} AnalyticsRequest;
	class AnalyticsManager : public Task<AnalyticsRequest> {
	public:
		AnalyticsManager();
		~AnalyticsManager();

		static AnalyticsManager *getSingleton(bool noCreate = false);

		void SubmitServer(INetServer *server);
	private:
		void UploadJson(const char *str);
		static void *TaskThread(OS::CThread *thread);
		void AppendMetricArrayToJson(json_t *object, MetricArrayValue value);
		void AppendMetricToJson(json_t *array, MetricInstance metric);
		void AppendMetricScalarToJson(json_t *object, std::string key, struct _Value value);
		static AnalyticsManager *mp_singleton;

		//MetricInstance m_metrics;

		std::vector<MetricInstance> m_metric_list;
	};
}
#endif //_OS_ANALYTICS_MGR_H