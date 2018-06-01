from peewee import *
from BaseModel_GameMaster import BaseModel_GameMaster


class GameGroup(BaseModel_GameMaster):
    class Meta:
        db_table = "grouplist"
    groupid = PrimaryKeyField()
    gameid = IntegerField()
    maxwaiting = IntegerField()
    name = TextField()
    other = TextField()