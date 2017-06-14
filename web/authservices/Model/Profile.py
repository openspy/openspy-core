from peewee import *
from BaseModel import BaseModel
from Model.User import User
import datetime

class GPBirthDay(Field):
	db_field = 'date'

	def db_value(self, value):
		if value != None:
			day = (value >> 24) & 0xFF
			month = (value >> 16) & 0xFF
			year = (value & 0xFFFF)
		else:
			return None
		return datetime.date(year,month,day)

	def python_value(self, value):
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

class Profile(BaseModel):
	class Meta:
		db_table = "profiles"
	id = PrimaryKeyField()
	user = ForeignKeyField(User, db_column="userid", related_name='profiles')
	nick = TextField()
	uniquenick = TextField()

	firstname = TextField()
	lastname = TextField()
	homepage = TextField()
	icquin = IntegerField()
	zipcode = IntegerField()
	sex = IntegerField()
	birthday = GPBirthDay()
	ooc = IntegerField()
	ind = IntegerField()
	inc = IntegerField()
	mar = IntegerField()
	chc = IntegerField()
	i1 = IntegerField()
	o1 = IntegerField()
	conn = IntegerField()
	lon = FloatField()
	lat = FloatField()

	aimname = TextField()
	countrycode = TextField()
	pic = IntegerField()

	namespaceid = IntegerField()
	deleted = BooleanField()
