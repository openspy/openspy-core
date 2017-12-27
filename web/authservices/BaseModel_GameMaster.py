from peewee import *
from playhouse.db_url import connect
import os
class BaseModel_GameMaster(Model):
    class Meta:
        #database = connect('mysql://openspy:openspy@localhost/GameTracker')
        database = connect("{}/{}".format(os.environ['SQL_CONN_STR'],"Gamemaster"))
