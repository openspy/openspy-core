from peewee import *
from BaseModel import BaseModel


class PersistData(BaseModel):
	class Meta:
		db_table = "persist_data"
	id = PrimaryKeyField()
	base64Data = TextField()
	profile = ForeignKeyField(Profile, db_column="profileid", related_name='persist_data')
    modified = TimestampField()
    persist_type = IntegerField()
    data_index = IntegerField()
