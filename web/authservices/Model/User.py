from peewee import *
from BaseModel import BaseModel


class User(BaseModel):
	class Meta:
		db_table = "users"
	id = PrimaryKeyField()
	email = TextField()
	email_verified = BooleanField()

	password = TextField()

	videocard1ram = IntegerField()
	videocard2ram = IntegerField()
	cpuspeed = IntegerField()
	cpubrandid = IntegerField()
	connectionspeed = IntegerField()
	hasnetwork = BooleanField()	


	publicmask = IntegerField()
	partnercode = IntegerField()


	deleted = BooleanField()