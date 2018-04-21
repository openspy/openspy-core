from peewee import *
from BaseModel import BaseModel
from Model.Profile import Profile

class PersistData(BaseModel):
    class Meta:
        db_table = "persist_data"
    id = PrimaryKeyField()
    base64Data = BlobField()
    profile = ForeignKeyField(Profile, db_column="profileid", related_name='fk_pd_profile_id')
    modified = DateTimeField()
    persist_type = IntegerField()
    data_index = IntegerField()
    game = ForeignKeyField(Profile, db_column="game", related_name='fk_pd_game_id')