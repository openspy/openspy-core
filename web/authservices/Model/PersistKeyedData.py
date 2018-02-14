from peewee import *
from BaseModel import BaseModel
from Model.Profile import Profile
from Model.Game import Game

class PersistKeyedData(BaseModel):
    class Meta:
        db_table = "persist_keyed_data"
    id = PrimaryKeyField()
    key_name = TextField()
    key_value = BlobField()
    profile = ForeignKeyField(Profile, db_column="profileid", related_name='fk_pkd_profile_id')
    game = ForeignKeyField(Game, db_column="gameid", related_name='fk_pkd_game_id')
    modified = DateTimeField()
    persist_type = IntegerField()
    data_index = IntegerField()