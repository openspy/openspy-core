from peewee import *
from playhouse.db_url import connect
_GLOBAL_MYSQL_CONNECTION= None
import os
class BaseModel(Model):
    class Meta:
        #database = connect('mysql://openspy:openspy@localhost/GameTracker')
        database = connect(os.environ['SQL_CONN_STR'])
