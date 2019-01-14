#ifndef _OS_KVREADER_H
#define _OS_KVREADER_H

#include <OS/OpenSpy.h>
#include <unordered_map>
#include <vector>
#include <iterator>
/*
	This is not thread safe!!

	Because of:
		GetHead
*/
namespace OS {
	class KVReader {
	public:
		KVReader();
		KVReader(std::string kv_pair, char delim = '\\', char line_delim=0, std::map<std::string, std::string> data_map = std::map<std::string, std::string>());
		~KVReader();
		std::string GetKeyByIdx(int n);
		std::string GetValueByIdx(int n);
		int 		GetValueIntByIdx(int n);
		std::pair<std::string, std::string> GetPairByIdx(int n);
		std::string 						GetValue(std::string key);
		int 								GetValueInt(std::string key);
		std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> GetHead() const;
		bool HasKey(std::string name);
		size_t Size() { return m_kv_map.size(); };
		std::map<std::string, std::string> GetKVMap() const;

		std::string ToString() const;
	private:
		int GetIndex(int n); //map internal index to external index
		bool IsDataKey(std::string key);
		std::vector<std::pair<std::string, std::string>>::const_iterator FindKey(std::string key);
		std::vector<std::pair<std::string, std::string>>::const_iterator FindValue(std::string key);
		std::vector< std::pair<std::string, std::string> > m_kv_map;
		char m_delimitor;
		char m_line_delimitor;
		std::map<std::string, std::string> m_data_key_map;
	};
}
#endif //_OS_KVREADER_H