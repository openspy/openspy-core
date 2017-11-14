#ifndef _DATACACHE_H
#define _DATACACHE_H

#include <OS/OpenSpy.h>
#include <time.h>
#include <map>
#include <utility>
namespace OS {
	typedef struct _DataCacheTimeout {
		int timeout_time_secs;
		int max_keys; //-1 if no max keys
	} DataCacheTimeout;
	template<typename K, typename V>
	class DataCache {
	public:

		DataCache(int num_threads, DataCacheTimeout timeout) : m_num_threads(num_threads), m_timeout(timeout) {
			m_thread_mutexes = (OS::CMutex **)malloc(sizeof(OS::CMutex *) * num_threads);
			for (int i = 0; i < num_threads; i++) {
				m_thread_mutexes[i] = OS::CreateMutex();
			}

			m_main_mutex = OS::CreateMutex();
			threadTempCache = new std::map<K, std::pair<V, struct timeval> >[num_threads];

		}

		~DataCache() {
			for (int i = 0; i < m_num_threads; i++) {
				delete m_thread_mutexes[i];
			}
			free((void *)m_thread_mutexes);
			delete threadTempCache;
			delete m_main_mutex;
		}

		void AddItem(int thread_index, K key, V item) {
			struct timeval time_now;
			m_thread_mutexes[thread_index]->lock();
			gettimeofday(&time_now, NULL);

			threadTempCache[thread_index][key] = std::pair<V, struct timeval>(item, time_now);

			m_thread_mutexes[thread_index]->unlock();
		}

		/*
		template<typename K, typename V>
		bool LookupItem(K key, V &val) {
			bool found = false;
			EnterCriticalSection(&merge_cs);
			if (mergedMap.find(key) != mergedMap.end()) {
				found = true;
				val = mergedMap[k];
			}
			LeaveCriticalSection(&merge_cs);
			return found;
		}
		*/

		void mergeMap(int thread_index) {
			m_main_mutex->lock();
			m_thread_mutexes[thread_index]->lock();
			typename std::map<K, std::pair<V, struct timeval> >::iterator it = threadTempCache[thread_index].begin();
			while (it != threadTempCache[thread_index].end()) {
				typename std::pair<K, std::pair<V, struct timeval> > p = *it;
				mergedMap[p.first] = p.second;
				it++;
			}
			threadTempCache[thread_index].clear();
			m_thread_mutexes[thread_index]->unlock();
			m_main_mutex->unlock();
		}

		void timeoutMap(int thread_index) {
			m_main_mutex->lock();
			struct timeval time_now;
			gettimeofday(&time_now, NULL);

			mergeMap(thread_index);

			typename std::map<K, std::pair<V, struct timeval> >::iterator it = mergedMap.begin();
			int idx = 0;
			while (it != mergedMap.end()) {
				typename std::pair<K, std::pair<V, struct timeval> > p = *it;
				if (time_now.tv_sec - p.second.second.tv_sec > m_timeout.timeout_time_secs || (++idx > m_timeout.max_keys && m_timeout.max_keys != -1)) {
					it = mergedMap.erase(it);
					continue;
				}
				it++;
			}
			m_main_mutex->unlock();
		}
	protected:
		std::map<K, std::pair<V, struct timeval> > *threadTempCache;

		std::map<K, std::pair<V, struct timeval> > mergedMap;

		OS::CMutex **m_thread_mutexes;
		OS::CMutex *m_main_mutex;

		int m_num_threads;
		DataCacheTimeout m_timeout;
	};
}
#endif //_DATACACHE_H