from peewee import *
from BaseModel import BaseModel


class User(BaseModel):
	PARTNERID_GAMESPY = 0
	PARTNERID_IGN = 10
	PARTNERID_EA = 20
	class Meta:
		db_table = "users"
	id = PrimaryKeyField()
	email = TextField()
	email_verified = BooleanField()
	password = TextField()
	videocard1ram = IntegerField()
	videocard2ram = IntegerField()
	cpuspeed = IntegerField()
	cpubrandid = IntegerField()
	connectionspeed = IntegerField()
	hasnetwork = BooleanField()	
	publicmask = IntegerField()
	partnercode = IntegerField()
	deleted = BooleanField()
	admin = BooleanField()

	def is_admin(self):
		return self.admin

	@staticmethod
	def is_email_valid(email): #disabled in AuthService because some PS2 games send invalid emails as a secret registration thing...
		return re.match('^[_a-zA-Z0-9-]+(\.[_a-zA-Z0-9-]+)*@[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)*(\.[a-zA-Z]{2,4})$', email) == None