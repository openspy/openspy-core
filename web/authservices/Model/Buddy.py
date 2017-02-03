from peewee import *
from BaseModel import BaseModel
from Model.Profile import Profile

class Buddy(BaseModel):
	class Meta:
		primary_key = False
		db_table = "buddies"
	to_profile = ForeignKeyField(Profile, db_column="to_profileid", related_name="buddy_to_profile_rel")
	from_profile = ForeignKeyField(Profile, db_column="from_profileid", related_name="buddy_from_profile_rel")