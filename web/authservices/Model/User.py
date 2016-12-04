from peewee import *
from BaseModel import BaseModel


class User(BaseModel):
	class Meta:
		db_table = "users"
	id = PrimaryKeyField()
	email = TextField()
	email_verified = BooleanField()

	password = TextField()

	partnercode = IntegerField()

	#created = TimestampField()
	deleted = BooleanField()