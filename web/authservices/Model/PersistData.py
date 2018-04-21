from peewee import *
from BaseModel import BaseModel
from Model.Profile import Profile
from Model.Game import Game

class PersistData(BaseModel):
    class Meta:
        db_table = "persist_data"
    id = PrimaryKeyField()
    base64Data = BlobField()
    profile = ForeignKeyField(Profile, db_column="profileid", related_name='fk_pd_profile_id')
    modified = DateTimeField()
    persist_type = IntegerField()
    data_index = IntegerField()
    game = ForeignKeyField(Game, db_column="gameid", related_name='fk_pd_game_id')