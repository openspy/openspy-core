from peewee import *
import datetime
class OSDate(Field):
	db_field = 'date'
	def db_value(self, value):
		return datetime.date(value.year, value.month, value.year)
	def python_value(self, value):
		return {'day': value.day, 'month': value.month, 'year': value.year}