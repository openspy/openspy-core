#include "AppConfig.h"

AppConfig::AppConfig(OS::Config *cfg, std::string appName) {
	mp_config = cfg;
	m_app_name = appName;
}
AppConfig::~AppConfig() {

}

std::vector<std::string> AppConfig::getDriverNames() {
	std::vector<std::string> names;
	OS::ConfigNode node;
	mp_config->GetRootNode().FindObjectField("applications", node);

	std::vector<OS::ConfigNode> applications_children = node.GetArrayChildren();
	std::vector<OS::ConfigNode>::iterator it = applications_children.begin(), it2;
	while (it != applications_children.end()) {
		OS::ConfigNode node = *it;
		std::vector<OS::ConfigNode> applications_children_sub = node.GetArrayChildren();
		if (node.GetKey().compare("application") == 0) {
			OS::ConfigNode name_node;
			if (node.FindAttribute("name", name_node)) {
				if (name_node.GetValue().compare(m_app_name) == 0) {
					OS::ConfigNode drivers_node = node.GetArrayChildren().front();

					std::vector<OS::ConfigNode> driver_children = drivers_node.GetArrayChildren();
					std::vector<OS::ConfigNode>::iterator it2 = driver_children.begin();
					while (it2 != driver_children.end()) {
						OS::ConfigNode driver_node = *it2;
						if (driver_node.FindAttribute("name", name_node)) {
							names.push_back(name_node.GetValue());
						}
						it2++;
					}
				}
				
			}
		}
		it++;
	}
	return names;
}
bool AppConfig::getDriverNode(std::string name, OS::ConfigNode &return_node) {
	std::vector<std::string> names;
	OS::ConfigNode node;
	mp_config->GetRootNode().FindObjectField("applications", node);

	std::vector<OS::ConfigNode> applications_children = node.GetArrayChildren();
	std::vector<OS::ConfigNode>::iterator it = applications_children.begin(), it2;
	while (it != applications_children.end()) {
		OS::ConfigNode node = *it;
		std::vector<OS::ConfigNode> applications_children_sub = node.GetArrayChildren();
		if (node.GetKey().compare("application") == 0) {
			OS::ConfigNode name_node;
			if (node.FindAttribute("name", name_node)) {
				if (name_node.GetValue().compare(m_app_name) == 0) {
					OS::ConfigNode drivers_node = node.GetArrayChildren().front();

					std::vector<OS::ConfigNode> driver_children = drivers_node.GetArrayChildren();
					std::vector<OS::ConfigNode>::iterator it2 = driver_children.begin();
					while (it2 != driver_children.end()) {
						OS::ConfigNode driver_node = *it2;
						it2++;
						if (driver_node.FindAttribute("name", name_node)) {
							if (name_node.GetValue().compare(name) == 0) {
								return_node = driver_node;
								return true;
							}
						}
					}
				}
			}
		}
		it++;
	}
	return false;
}
bool AppConfig::GetVariableString(std::string driverName, std::string name, std::string &out) {
	std::map<std::string, std::string> variables =  getCombinedVariables(driverName);
	if (variables.find(name) != variables.end()) {
		out = variables[name];
		return true;
	}
	return false;
}
bool AppConfig::GetVariableInt(std::string driverName, std::string name, int &out) {
	std::string var_out;
	if (GetVariableString(driverName, name, var_out)) {
		out = atoi(var_out.c_str());
		return true;
	}
	return false;
}
std::map<std::string, std::string> AppConfig::getCombinedVariables(std::string driverName) {
	std::map<std::string, std::string> ret;

	OS::ConfigNode variables_node, name_attribute;
	std::map<std::string, std::string> global_variables;
	mp_config->GetRootNode().FindObjectField("variables", variables_node);

	std::vector<OS::ConfigNode> global_variables_children = variables_node.GetArrayChildren();
	std::vector<OS::ConfigNode>::iterator it = global_variables_children.begin();
	while (it != global_variables_children.end()) {
		OS::ConfigNode variable_node = *it;
		std::string v = GetVariableValue(variable_node);

		
		variable_node.FindAttribute("name", name_attribute);
		ret[name_attribute.GetValue()] = v;
		it++;
	}

	OS::ConfigNode driver_node;
	if (getDriverNode(driverName, driver_node)) {
		std::vector<OS::ConfigNode> driver_children = driver_node.GetArrayChildren();
		it = driver_children.begin();
		while (it != driver_children.end()) {
			variables_node = *it;
			if (variables_node.GetKey().compare("variables") == 0) {
				driver_children = variables_node.GetArrayChildren();
				it = driver_children.begin();
				while (it != driver_children.end()) {
					OS::ConfigNode variable_node = *it;
					std::string v = GetVariableValue(variable_node);
					variable_node.FindAttribute("name", name_attribute);
					ret[name_attribute.GetValue()] = v;
					it++;
				}
				break;
			}
			it++;
		}
	}

	return ret;
}
std::string AppConfig::GetVariableValue(OS::ConfigNode node) {
	std::string ret;
	OS::ConfigNode attribute;
	if (node.FindAttribute("type", attribute)) {
		if (attribute.GetValue().compare("env") == 0) {
			const char *env = getenv(node.GetValue().c_str());;
			if(env)
				ret = env;
			goto end;
		}
	}
	ret = node.GetValue();
	end:
	return ret;
}
std::vector<OS::Address> AppConfig::GetDriverAddresses(std::string driverName, bool &proxyFlag) {
	std::vector<OS::Address> ret;
	OS::Address address;
	OS::ConfigNode node;
	if (getDriverNode(driverName, node)) {
		std::vector<OS::ConfigNode> driver_children = node.GetArrayChildren();
		std::vector<OS::ConfigNode>::iterator it = driver_children.begin();
		while (it != driver_children.end()) {
			OS::ConfigNode node = *it;

			if (node.GetKey().compare("addresses") == 0) {
				std::vector<OS::ConfigNode> address_nodes = node.GetArrayChildren();
				std::vector<OS::ConfigNode>::iterator it2 = address_nodes.begin();
				while (it2 != address_nodes.end()) {
					OS::ConfigNode node2 = *it2;
					std::vector<OS::ConfigNode> address_info_nodes = node2.GetArrayChildren();
					std::vector<OS::ConfigNode>::iterator it3 = address_info_nodes.begin();
					std::string ip;
					int port;
					while (it3 != address_info_nodes.end()) {
						OS::ConfigNode node3 = *it3;
						std::vector<OS::ConfigNode> address_detail_nodes = node3.GetArrayChildren();
						if (node3.GetKey().compare("ip") == 0) {
							ip = node3.GetValue();
						}
						else if (node3.GetKey().compare("port") == 0) {
							port = node3.GetValueInt();
						} else if(node3.GetKey().compare("proxyHeaders") == 0) {
							proxyFlag = atoi(node3.GetValue().c_str()) != 0;
						}
						it3++;
					}
					address = OS::Address(ip);
					address.port = htons(port);
					ret.push_back(address);
					it2++;
				}
			}
			it++;
		}
	}
	return ret;
}