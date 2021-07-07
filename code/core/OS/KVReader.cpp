#include "KVReader.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <math.h>
#include <OS/Buffer.h>
namespace OS {
	KVReader::KVReader() {

	}
	KVReader::KVReader(std::string kv_pair, char delim, char line_delim, std::map<std::string, std::string> data_map) {
		m_delimitor = delim;
		m_line_delimitor = line_delim;
		m_data_key_map = data_map;
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

			while ((key.length() > 0 && IsDataKey(key)) || std::getline(line_ss, token, delim)) {
				if (i % 2 == 0) {
					key = (token);
				}
				else {
					value = (token);
					if (!HasKey(key)) {
						if (IsDataKey(key)) {
							std::map<std::string, std::string>::iterator it = m_data_key_map.find(key);
							size_t data_len = (size_t)GetValueInt((*it).second);
							
							if (data_len > kv_pair.length()) { //not a perfect check, but read will not mess up when reading too much
								break;
							}
						
							OS::Buffer binary_data = OS::Buffer(data_len);
							line_ss.read((char *)binary_data.GetHead(), data_len);
							line_ss.seekg(1, std::ios_base::cur);
							value = std::string((const char *)binary_data.GetHead(), data_len);
							m_kv_map.push_back(std::pair<std::string, std::string>(key, value));
						}
						else {
							m_kv_map.push_back(std::pair<std::string, std::string>(key, value));
						}
					}
					key = "";
				}
				i++;
			}
			if (line_delim == 0) {
				value = "";
				break;
			}
		}
		if (key.length()) {
			m_kv_map.push_back(std::pair<std::string, std::string>(key, value));
		}
	}
	bool KVReader::IsDataKey(std::string key) {
		return m_data_key_map.find(key) != m_data_key_map.end();
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
		if (m_kv_map.size() <= (unsigned int)n)
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
	std::string KVReader::ToString(bool values_only) const {
		std::string ret;
		std::vector<std::pair<std::string, std::string>>::const_iterator it = m_kv_map.cbegin();
		while (it != m_kv_map.cend()) {
			std::pair<std::string, std::string> p = *it;
			if(values_only) {
				ret += m_delimitor + p.second;
			} else {
				ret += m_delimitor + p.first + m_delimitor + p.second;
			}
			
			if (m_line_delimitor != 0) {
				ret += m_line_delimitor;
			}
			it++;
		}
		return ret;
	}
}