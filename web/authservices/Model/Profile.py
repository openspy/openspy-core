from peewee import *
from BaseModel import BaseModel
from Model.User import User
from Model.Fields.OSDate import OSDate
import datetime

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
	birthday = OSDate()
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
