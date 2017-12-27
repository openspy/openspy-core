from peewee import *
from BaseModel_GameMaster import BaseModel_GameMaster


class GameGroup(BaseModel_GameMaster):
    class Meta:
        db_table = "grouplist"
    groupid = PrimaryKeyField()
    gameid = IntegerField()
    maxwaiting = IntegerField()
    numplayers = IntegerField()
    numwaiting = IntegerField()
    numservers = IntegerField()
    password = IntegerField()
    updatetime = IntegerField()
    name = TextField()
    other = TextField()