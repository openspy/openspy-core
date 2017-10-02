#include "AnalyticsMgr.h"
#include <curl/curl.h>
namespace OS {
	AnalyticsManager *AnalyticsManager::mp_singleton = NULL;
	AnalyticsManager::AnalyticsManager() {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(AnalyticsManager::TaskThread, this, true);
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
		mp_mutex->lock();
		m_metric_list.push_back(server->GetMetrics());
		mp_mutex->unlock();

		mp_thread_poller->signal();
	}
	void AnalyticsManager::AppendMetricArrayToJson(json_t *object, MetricArrayValue value) {
		std::vector<std::pair<MetricType, struct _Value> > values = value.values;
		std::vector<std::pair<MetricType, struct _Value> >::iterator it = values.begin();
		while(it != values.end()) {
			std::pair<MetricType, struct _Value> p = *it;
			if(p.second.type == MetricType_Array)  {
				json_t *sub_object = json_object();
				AppendMetricArrayToJson(sub_object, p.second.arr_value);
				json_object_set_new(object, p.second.key.c_str(), sub_object);
			} else {
				AppendMetricScalarToJson(object, p.second.key, p.second);
			}
			
			it++;
		}
	}
	void AnalyticsManager::AppendMetricToJson(json_t *object, MetricInstance metric) {
		if(metric.value.type == MetricType_Array) {
			AppendMetricArrayToJson(object, metric.value.arr_value);
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
	void *AnalyticsManager::TaskThread(OS::CThread *thread) {
		AnalyticsManager *task = (AnalyticsManager *)thread->getParams();
		while(task->mp_thread_poller->wait()) {
			task->mp_mutex->lock();

			std::vector<MetricInstance>::iterator it = task->m_metric_list.begin();
			while(it != task->m_metric_list.end()) {

				MetricInstance instance = *it;
				json_t *array = json_object();
				task->AppendMetricToJson(array, instance);

				char *json_data = json_dumps(array, JSON_INDENT(0));
				task->UploadJson(json_data);

				free((void *)json_data);	
				json_decref(array);
				it++;
			}
			task->m_metric_list.clear();
			task->mp_mutex->unlock();
		}
	}
	void AnalyticsManager::UploadJson(const char *str) {
		CURL *curl = curl_easy_init();
		CURLcode res;
		if(curl) {
			struct curl_slist *chunk = NULL;
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_ANALYTICS_MGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, str);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSAnalyticsMgr");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			res = curl_easy_perform(curl);

			curl_easy_cleanup(curl);
			curl_slist_free_all(chunk);
		}
	}
}