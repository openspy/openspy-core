from peewee import *
from playhouse.db_url import connect
_GLOBAL_MYSQL_CONNECTION= None
class BaseModel(Model):
    class Meta:
        database = connect('mysql://root@localhost/GameTracker')
