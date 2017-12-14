#ifndef _N_DATE_H
#define _N_DATE_H
#include <stdint.h>
#include <jansson.h>
namespace OS {
	class Date {
	public:
		Date(uint16_t year, uint8_t month, uint8_t day) : m_year(year), m_month(month), m_day(day) { };
		Date() : m_year(0), m_month(0), m_day(0) { };
		
		json_t *GetJson();
		
		static Date GetDateFromJSON(json_t *object);
		static Date GetDateFromGPValue(int value);
		int GetGPDate();
		uint16_t GetYear() { return m_year; };
		uint8_t GetMonth() { return m_month; };
		uint8_t GetDay() { return m_day; };
	private:
		uint16_t m_year;
		uint8_t m_month;
		uint8_t m_day;
	};
}
#endif //_N_DATE_H