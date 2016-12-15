from peewee import *
from BaseModel import BaseModel
from User import User

class Profile(BaseModel):
	class Meta:
		db_table = "profiles"
	id = PrimaryKeyField()
	user = ForeignKeyField(User, db_column="userid", related_name='profiles')
	nick = TextField()
	uniquenick = TextField()

	firstname = TextField()
	lastname = TextField()
	icquin = IntegerField()

	namespaceid = IntegerField()
	deleted = BooleanField()