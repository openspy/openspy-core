from peewee import *
from BaseModel import BaseModel
from Model.Profile import Profile

class PersistData(BaseModel):
    class Meta:
        db_table = "persist_data"
    id = PrimaryKeyField()
    base64Data = BlobField()
    profile = ForeignKeyField(Profile, db_column="profileid", related_name='persist_data')
    modified = DateTimeField()
    persist_type = IntegerField()
    data_index = IntegerField()