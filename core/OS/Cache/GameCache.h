#ifndef OS_GAMECACHE_H
#define OS_GAMECACHE_H
#include <OS/OpenSpy.h>
#include <OS/Cache/DataCache.h>
namespace OS {
	typedef struct _GameCacheKey {
		int id;
		std::string gamename;
		bool operator<(const struct _GameCacheKey &rhs) const { return id < rhs.id; };
		bool operator>(const struct _GameCacheKey &rhs) const { return id > rhs.id; };
	} GameCacheKey;
	class GameCache : public DataCache<GameCacheKey, GameData> {
		public:
			GameCache(int num_threads, DataCacheTimeout timeout) : DataCache<GameCacheKey, GameData>(num_threads, timeout) {

			}

			bool LookupGameByID(int id, GameData &val) {
				bool found = false;
				m_main_mutex->lock();
				std::map<GameCacheKey, std::pair<GameData, struct timeval> >::iterator it = mergedMap.begin();
				while (it != mergedMap.end()) {
					std::pair<GameCacheKey, std::pair<GameData, struct timeval> > p = *it;
					if (p.first.id = id) {
						val = p.second.first;
						break;
					}
					it++;
				}
				m_main_mutex->unlock();
				return found;
			}

			bool LookupGameByName(std::string gamename, GameData &val) {
				bool found = false;
				m_main_mutex->lock();
				std::map<GameCacheKey, std::pair<GameData, struct timeval> >::iterator it = mergedMap.begin();
				while (it != mergedMap.end()) {
					std::pair<GameCacheKey, std::pair<GameData, struct timeval> > p = *it;
					if (p.first.gamename.compare(gamename) == 0) {
						found = true;
						val = p.second.first;
						break;
					}
					it++;
				}
				m_main_mutex->unlock();
				return found;
			}
	};
}

#endif //OS_GAMECACHE_H