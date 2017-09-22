#include "AnalyticsMgr.h"

namespace OS {
	AnalyticsManager *AnalyticsManager::mp_singleton = NULL;
	AnalyticsManager::AnalyticsManager() {

	}
	AnalyticsManager::~AnalyticsManager() {
		
	}

	AnalyticsManager *AnalyticsManager::getSingleton() {
		if(mp_singleton == NULL) {
			mp_singleton = new AnalyticsManager();
		}
		return mp_singleton;
	}

	void AnalyticsManager::SubmitServer(INetServer *server) {
		m_metrics = server->GetMetrics();
		UploadMetricTree(m_metrics);

		
	}
	void AnalyticsManager::AppendMetricArrayToJson(json_t *object, struct _Value value) {
		std::vector<std::pair<MetricType, struct _Value> > values = value.arr_value.values;
		std::vector<std::pair<MetricType, struct _Value> >::iterator it = values.begin();
		while(it != values.end()) {
			std::pair<MetricType, struct _Value> p = *it;
			if(p.first == MetricType_Array)  {
				json_t *sub_object = json_object();
				AppendMetricArrayToJson(sub_object, p.second);
				json_object_set(object, p.second.key.c_str(), sub_object);
			} else {
				AppendMetricScalarToJson(object, p.second.key, p.second);
			}
			
			it++;
		}
	}
	void AnalyticsManager::AppendMetricToJson(json_t *object, MetricInstance metric) {
		if(metric.value.type == MetricType_Array) {
			AppendMetricArrayToJson(object, metric.value);
		} else {
			AppendMetricScalarToJson(object, metric.key, metric.value);
		}
	}
	void AnalyticsManager::AppendMetricScalarToJson(json_t *object, std::string key, struct _Value value) {
		switch(value.type) {
			case MetricType_Integer:
				json_object_set_new(object, key.c_str(), json_integer(value.value._int));
				break;
			case MetricType_Float:
				json_object_set_new(object, key.c_str(), json_real(value.value._float));
				break;
			case MetricType_UnixTime:
				break;
			case MetricType_String:
				json_object_set_new(object, key.c_str(), json_string(value.value._str.c_str()));
				break;
		}
	}
	void AnalyticsManager::UploadMetricTree(MetricInstance metric) {
		json_t *array = json_object();
		AppendMetricToJson(array, metric);

		//dump blah blah
		char *json_data = json_dumps(array, JSON_INDENT(0));
		printf("JSON: %s\n", json_data);
	}
}