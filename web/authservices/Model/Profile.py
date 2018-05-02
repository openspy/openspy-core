from peewee import *
from BaseModel import BaseModel
from Model.User import User
from Model.Fields.OSDate import OSDate
import datetime

class Profile(BaseModel):
	NAMESPACEID_IGN = 15
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

	@staticmethod
	def is_nick_valid(nick, namespaceid):
		bad_start_chars = ["@", "+", "#", ":"]

		if len(nick) < 2 or len(nick) > 20:
			return False

		if nick[0] in bad_start_chars:
			return False

		bad_chars = ["\\", ","]
		for n in nick:
			if n in bad_chars:
				return False
			if namespaceid == Profile.NAMESPACEID_IGN:
				if n not in "1234567890#_-`()$-=;/@+&abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ":
					return False
			else:
				if ord(n) < 34 or ord(n) > 126:
					return False
		return True