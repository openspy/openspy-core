from peewee import *
import datetime
#GameSpy Presence date field
class GPBirthDay(Field):
	db_field = 'date'

	def python_value(self, value):
		if value != None:
			day = (value >> 24) & 0xFF
			month = (value >> 16) & 0xFF
			year = (value & 0xFFFF)
		else:
			return None
		if year == 0:
			year = 1900
		if day == 0:
			day = 1
		if month == 0: 
			month = 1
		return datetime.date(year,month,day)

	def db_value(self, value):
		day = 0
		month = 0
		year = 0
		if value != None:
			day = value.day
			month = value.month
			year = value.year

		val = 0
		val |= (day << 24)
		val |= (month << 16)
		val |= (year)
		return val # convert str to UUID