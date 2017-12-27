from peewee import *
from BaseModel_GameMaster import BaseModel_GameMaster


class Game(BaseModel_GameMaster):
	class Meta:
		db_table = "games"
	id = PrimaryKeyField()
	gamename = TextField()
	secretkey = TextField()
	description = TextField()
	queryport = IntegerField()
	backendflags = IntegerField()
	disabledservices = IntegerField()
	keylist = TextField()
	keytypelist = TextField()