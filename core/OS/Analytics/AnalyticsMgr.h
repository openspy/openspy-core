#ifndef _OS_ANALYTICS_MGR_H
#define _OS_ANALYTICS_MGR_H
#include <jansson.h>
#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/Net/NetDriver.h>
#include <OS/Net/NetServer.h>
#include "Metric.h"

#define ANALYTICS_SUBMIT_TIME 15
namespace OS {
	class AnalyticsManager {
	public:
		AnalyticsManager();
		~AnalyticsManager();

		static AnalyticsManager *getSingleton();

		void SubmitServer(INetServer *server);
	private:
		void UploadMetricTree(MetricInstance metric);
		void AppendMetricArrayToJson(json_t *object, MetricArrayValue value);
		void AppendMetricToJson(json_t *array, MetricInstance metric);
		void AppendMetricScalarToJson(json_t *object, std::string key, struct _Value value);
		static AnalyticsManager *mp_singleton;

		MetricInstance m_metrics;
	};
}
#endif //_OS_ANALYTICS_MGR_H