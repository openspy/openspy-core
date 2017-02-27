#ifndef _OS_KVREADER_H
#define _OS_KVREADER_H

#include <OS/OpenSpy.h>
#include <unordered_map>
#include <iterator>
/*
	This is not thread safe!!
*/
namespace OS {
	class KVReader {
	public:
		KVReader(std::string kv_pair, char delim = '\\');
		~KVReader();
		std::string GetKey(int n);
		std::string GetValue(int n);
		int 		GetValueInt(int n);
		std::pair<std::string, std::string> GetPair(int n);
		std::string 						GetValue(std::string key);
		int 								GetValueInt(std::string key);
		std::pair<std::unordered_map<std::string, std::string>::const_iterator, std::unordered_map<std::string, std::string>::const_iterator> GetHead() const;
		bool HasKey(std::string name);
	private:
		int GetIndex(int n); //map internal index to external index
		std::unordered_map<std::string, std::string> m_kv_map;
	};
}
#endif //_OS_KVREADER_H