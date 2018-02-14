from peewee import *
from BaseModel import BaseModel


class PersistKeyedData(BaseModel):
	class Meta:
		db_table = "persist_keyed_data"
	id = PrimaryKeyField()
    key_name = TextField()
	key_value = TextField()
	profile = ForeignKeyField(Profile, db_column="profileid", related_name='persist_data')
    game = ForeignKeyField(Game, db_column="gameid", related_name='persist_data')
    modified = TimestampField()
    persist_type = IntegerField()
    data_index = IntegerField()
