#include "KVReader.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <math.h>
namespace OS {
	KVReader::KVReader() {

	}
	KVReader::KVReader(std::string kv_pair, char delim, char line_delim) {
		m_delimitor = delim;
		std::string token, key, value, line;
		int i = 0;

		std::stringstream ss, line_ss;
		if (kv_pair[0] == delim) {
			kv_pair = kv_pair.substr(1, kv_pair.length() - 1);
		}
		ss.str(kv_pair);

		while (line_delim == 0 || std::getline(ss, line, line_delim)) {
			if (line_delim == 0) {
				line_ss = std::stringstream(ss.str());
			}
			else {
				line_ss = std::stringstream(line);
			}
			i = 0;
			while (std::getline(line_ss, token, delim)) {
				if (i % 2 == 0) {
					key = OS::strip_whitespace(token);
				}
				else {
					value = OS::strip_whitespace(token);
					if (!HasKey(key)) {
						m_kv_map.push_back(std::pair<std::string, std::string>(key, value));
					}
				}
				i++;
			}
			if (line_delim == 0) {
				break;
			}
		}
	}
	KVReader::~KVReader() {

	}
	std::string KVReader::GetKeyByIdx(int n) {
		return GetPairByIdx(n).first;

	}
	std::string KVReader::GetValueByIdx(int n) {
		return GetPairByIdx(n).second;
	}
	int 		KVReader::GetValueIntByIdx(int n) {
		return atoi(GetValueByIdx(n).c_str());
	}
	std::pair<std::string, std::string> KVReader::GetPairByIdx(int n) {
		n = GetIndex(n);
		if (m_kv_map.size() < (unsigned int)n)
			return std::pair<std::string, std::string>("", "");
		std::vector<std::pair<std::string, std::string>>::const_iterator it = m_kv_map.begin();
		std::advance(it, n);

		return *it;
	}
	std::string KVReader::GetValue(std::string key) {
		std::vector<std::pair<std::string, std::string>>::const_iterator it = FindKey(key);
		if (it == m_kv_map.end()) {
			return "";
		}

		return (*it).second;
	}
	int	KVReader::GetValueInt(std::string key) {
		std::vector<std::pair<std::string, std::string>>::const_iterator it = FindKey(key);
		if (it == m_kv_map.end()) {
			return 0;
		}

		return atoi((*it).second.c_str());
	}
	std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> KVReader::GetHead() const {
		return std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator>(m_kv_map.begin(), m_kv_map.end());
	}
	int KVReader::GetIndex(int n) {
		return n;//abs((int)(n - m_kv_map.size())) - 1;
	}
	bool KVReader::HasKey(std::string name) {
		return FindKey(name) != m_kv_map.cend();
	}
	std::vector<std::pair<std::string, std::string>>::const_iterator KVReader::FindKey(std::string key) {
		std::vector<std::pair<std::string, std::string>>::const_iterator it = m_kv_map.cbegin();
		while (it != m_kv_map.cend()) {
			std::pair<std::string, std::string> p = *it;
			if (p.first.compare(key) == 0) {
				return it;
			}
			it++;
		}
		return m_kv_map.cend();
	}
	std::vector<std::pair<std::string, std::string>>::const_iterator KVReader::FindValue(std::string key) {
		std::vector<std::pair<std::string, std::string>>::const_iterator it = m_kv_map.cbegin();
		while (it != m_kv_map.cend()) {
			std::pair<std::string, std::string> p = *it;
			if (p.second.compare(key) == 0) {
				return it;
			}
			it++;
		}
		return m_kv_map.cend();
	}
	std::map<std::string, std::string> KVReader::GetKVMap() const {
		std::map<std::string, std::string> ret;
		std::vector<std::pair<std::string, std::string>>::const_iterator it = m_kv_map.cbegin();
		while (it != m_kv_map.cend()) {
			std::pair<std::string, std::string> p = *it;
			ret[p.first] = p.second;
			it++;
		}
		return ret;
	}
	std::string KVReader::ToString() const {
		std::string ret = "" + m_delimitor;
		std::vector<std::pair<std::string, std::string>>::const_iterator it = m_kv_map.cbegin();
		while (it != m_kv_map.cend()) {
			std::pair<std::string, std::string> p = *it;
			ret += p.first + m_delimitor + p.second;
			it++;
		}
		return ret;
	}
}