#include "Date.h"

namespace OS {
	json_t *Date::GetJson() {
		json_t* obj = json_object();
		
		json_object_set_new(obj, "day", json_integer(m_day));
		json_object_set_new(obj, "month", json_integer(m_month));
		json_object_set_new(obj, "year", json_integer(m_year));
		
		return obj;
	}
	Date Date::GetDateFromJSON(json_t *object) {
		Date date;
		json_t *obj = json_object_get(object, "day");
		date.m_day = (uint8_t)json_integer_value(obj);
		
		obj = json_object_get(object, "month");
		date.m_month = (uint8_t)json_integer_value(obj);
		
		obj = json_object_get(object, "year");
		date.m_year = (uint16_t)json_integer_value(obj);
		
		return date;
	}
	Date Date::GetDateFromGPValue(int value) {
		Date date;
		date.m_day = (value >> 24) & 0xFF;
		date.m_month = (value >> 16) & 0xFF;
		date.m_year = (value & 0xFFFF);
		return date;
	}
	
	int Date::GetGPDate() {
		int val = 0;
		val |= (m_day << 24);
		val |= (m_month << 16);
		val |= (m_year);
		return val;
	}
}