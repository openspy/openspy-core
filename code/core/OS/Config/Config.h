#ifndef _OS_CONFIG_PARSER
#define _OS_CONFIG_PARSER
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <pugixml.hpp>
namespace OS {

	enum ENodeType {
		ENodeType_Object,
		ENodeType_Array,
		ENodeType_Variable,
	};
	class ConfigNode {
	public:
		ConfigNode() { };

		ConfigNode(ENodeType node_type, std::string key = "", std::string value = "") { m_node_type = node_type; m_value = value; m_key = key; };
		void AddAttribute(std::string attribute_name, ConfigNode node) { m_attributes[attribute_name] = node; };
		int GetValueInt() { return atoi(m_value.c_str()); };
		std::string GetKey() { return m_key; };
		void SetKey(std::string k) { m_key = k; }

		std::string GetValue() { return m_value; };
		void SetValue(std::string v) { m_value = v; }


		void AddArrayItem(ConfigNode item);
		std::vector<ConfigNode> GetArrayChildren() { return m_array_children; };
		void SetObjectField(std::string key, ConfigNode item);
		std::map<std::string, ConfigNode> GetObjectFields() { return m_object_fields; };

		bool FindAttribute(std::string attribute_name, ConfigNode& attribute) {
			std::map<std::string, ConfigNode>::iterator it = m_attributes.find(attribute_name);
			if (it != m_attributes.end()) {
				attribute = (*it).second;
				return true;
			}
			return false;
		}
		bool FindObjectField(std::string field_name, ConfigNode& attribute) {
			std::map<std::string, ConfigNode>::iterator it = m_object_fields.find(field_name);
			if (it != m_object_fields.end()) {
				attribute = (*it).second;
				return true;
			}
			return false;
		}

	private:
		std::map<std::string, ConfigNode> m_attributes;

		std::vector<ConfigNode> m_array_children;
		std::map<std::string, ConfigNode> m_object_fields;
	
		std::string m_key, m_value;
		ENodeType m_node_type;
	};
	class Config {
	public:
		Config(std::string path);
		~Config();
		ConfigNode GetRootNode() { return m_root_node; };
	private:
		ConfigNode m_root_node;
		void parse_document(std::string path);
		void handle_variable_node(OS::ConfigNode &node,pugi::xml_node xml_node);
		void handle_variable_siblings(OS::ConfigNode &node, pugi::xml_node xml_node);
	};
}

#endif