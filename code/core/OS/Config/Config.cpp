#include "AppConfig.h"
#include "Config.h"
#include <iostream>
#include <sstream>
namespace OS {
	Config::Config(std::string filename) {
		parse_document(filename);
	}
	Config::~Config() {
		
	}
	void Config::parse_document(std::string path) {
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(path.c_str());
		if (!result)
			return;

		m_root_node = OS::ConfigNode();

		OS::ConfigNode root_variables_node(ENodeType_Object);

		pugi::xml_node_iterator it = doc.children().begin();
		while (it != doc.children().end()) {
			pugi::xml_node node = *it;
			OS::ConfigNode child_node;
			handle_variable_node(child_node, node);
			m_root_node.SetObjectField(child_node.GetKey(), child_node);
			it++;
		}
	}
	void Config::handle_variable_siblings(OS::ConfigNode &node, pugi::xml_node xml_node) {
		//xml_node.
		//int num_sibblings = std::distance(xml_node.next_sibling().begin(), xml_node.children().end());
		//printf("node: %s\nnum siblings: %d\n", xml_node.name(), num_sibblings);
	}
	void Config::handle_variable_node(OS::ConfigNode &node, pugi::xml_node xml_node) {
		int num_children = std::distance(xml_node.children().begin(), xml_node.children().end());
		pugi::xml_node_iterator it;
		if (!strlen(xml_node.name())) return;


		pugi::xml_attribute_iterator ia = xml_node.attributes().begin();
		while (ia != xml_node.attributes().end()) {
			pugi::xml_attribute attribute = (*ia);
			OS::ConfigNode attribute_node(ENodeType_Variable, attribute.name(), attribute.as_string());
			node.AddAttribute(attribute.name(), attribute_node);
			ia++;
		}

		if (num_children > 0) {
			OS::ConfigNode array_node(ENodeType_Array);
			it = xml_node.begin();
			while (it != xml_node.children().end()) {
				pugi::xml_node child_node = *it;
				OS::ConfigNode array_item_node;
				if (!strlen(child_node.name())) break;
				handle_variable_node(array_item_node, child_node);
				node.AddArrayItem(array_item_node);
				it++;
			}
			array_node.SetKey(xml_node.name());
		}

		node.SetKey(xml_node.name());
		node.SetValue(xml_node.text().as_string());
	}

	void ConfigNode::AddArrayItem(ConfigNode item) {
		m_array_children.push_back(item);
	}
	void ConfigNode::SetObjectField(std::string name, ConfigNode item) {
		m_object_fields[name] = item;
	}
}