from cgi import parse_qs, escape

import base64
import calendar
from datetime import datetime
import json

from BaseService import BaseService

from lib.Exceptions.OS_BaseException import OS_BaseException
from lib.Exceptions.OS_CommonExceptions import *

from playhouse.shortcuts import model_to_dict, dict_to_model

from Model.Profile import Profile
from Model.PersistData import PersistData
from Model.PersistKeyedData import PersistKeyedData

class PersistService(BaseService):
    def __init__(self):
        BaseService.__init__(self)

    def handle_new_game(self, request_body):
        response = {}
        data = {'game_identifier': "NOT_IMPLEMENTED"}
        response['success'] = True
        response["data"] = data
        return response
    def handle_update_game(self, request_body):
        response = {}
        response['success'] = True
        response["data"] = {}
        return response
    def set_persist_raw_data(self, persist_data):
        profile = None
        try:
            profile = Profile.select().where((Profile.id == persist_data["profileid"])).get()
        except (Profile.DoesNotExist) as e:
            pass
        try:

            data_entry = PersistData.select().where((PersistData.data_index == persist_data["data_index"]) & (PersistData.persist_type == persist_data["data_type"]) & (PersistData.profileid == profile.id)).get()
            data_entry.base64Data = persist_data["data"]
            data_entry.modified = datetime.utcnow()

            data_entry.save()
        except (PersistData.DoesNotExist, Profile.DoesNotExist) as e:
            data_entry = PersistData.create(base64Data=persist_data["data"], data_index=persist_data["data_index"], persist_type=persist_data["data_type"], modified = datetime.utcnow(), profileid = persist_data["profileid"])

        ret = model_to_dict(data_entry)
        del ret["profile"]
        
        ret["modified"] = calendar.timegm(ret["modified"].utctimetuple())
        return ret

    def get_keyed_data(self, persist_data, key):
        ret = {}
        try:
            profile = Profile.select().where((Profile.id == persist_data["profileid"])).get()
        except (Profile.DoesNotExist) as e:
            pass
        try:

            data_entry = PersistKeyedData.select().where((PersistKeyedData.key_name == key) & (PersistKeyedData.data_index == persist_data["data_index"]) & (PersistKeyedData.persist_type == persist_data["data_type"]) & (PersistKeyedData.profileid == profile.id)).get()
            ret = model_to_dict(data_entry)
            del ret["profile"]
        except (PersistKeyedData.DoesNotExist, Profile.DoesNotExist) as e:
            pass        
       
        ret["modified"] = calendar.timegm(ret["modified"].utctimetuple())
        return ret
    def update_or_create_keyed_data(self, persist_data, key, value):
        profile = None
        try:
            profile = Profile.select().where((Profile.id == persist_data["profileid"])).get()
        except (Profile.DoesNotExist) as e:
            pass
        try:

            data_entry = PersistKeyedData.select().where((PersistKeyedData.key_name == key) & (PersistKeyedData.data_index == persist_data["data_index"]) & (PersistKeyedData.persist_type == persist_data["data_type"]) & (PersistKeyedData.profileid == profile.id)).get()
            data_entry.key_value = value
            data_entry.modified = datetime.utcnow()

            data_entry.save()
        except (PersistKeyedData.DoesNotExist, Profile.DoesNotExist) as e:
            data_entry = PersistKeyedData.create(key_name=key,key_value=value, data_index=persist_data["data_index"], persist_type=persist_data["data_type"], modified = datetime.utcnow(), profileid = persist_data["profileid"])

        ret = model_to_dict(data_entry)
        del ret["profile"]
        
        ret["modified"] = calendar.timegm(ret["modified"].utctimetuple())
        return ret

    def set_persist_keyed_data(self, persist_data):
        ret = {}
        d = datetime.utcnow()
        ret["modified"] = calendar.timegm(d.utctimetuple())
        ret["success"] = True
        print("SET: {}\n".format(persist_data))
        for key in persist_data["keyList"]:
            self.update_or_create_keyed_data(persist_data, key, persist_data["keyList"][key])
        return ret
    def get_persist_raw_data(self, persist_data):
        profile = None
        try:
            profile = Profile.select().where((Profile.id == persist_data["profileid"])).get()
        except (Profile.DoesNotExist) as e:
            pass
        ret = None
        try:
            data_entry = PersistData.select().where((PersistData.data_index == persist_data["data_index"]) & (PersistData.persist_type == persist_data["data_type"]) & (PersistData.profileid == profile.id)).get()
            ret = model_to_dict(data_entry)
            ret["modified"] = calendar.timegm(ret["modified"].utctimetuple())
            del ret["profile"]
        except (PersistData.DoesNotExist, Profile.DoesNotExist) as e:
            pass
        
        return ret
    def handle_set_data(self, request_body):
        response = {}
        search_params = {"data_index": request_body["data_index"], "data_type": request_body["data_type"], "game_id": request_body["game_id"], "profileid": request_body["profileid"]}
        if "data" in request_body:
            save_data = {"data": request_body["data"], **search_params}
            response = self.set_persist_raw_data(save_data)
        elif "keyList" in request_body:
            save_data = {"keyList": request_body["keyList"], **search_params}
            self.set_persist_keyed_data(save_data)

        response['success'] = True
        return response
    def handle_get_data(self, request_body):
        response = {"success": False}
        persist_req_data = {"data_index": request_body["data_index"], "data_type": request_body["data_type"], 
        "game_id": request_body["game_id"], "profileid": request_body["profileid"]}
        if "keyList" not in request_body:
            persist_data = self.get_persist_raw_data(persist_req_data)
            if persist_data != None:
                response["success"] = True
                response["data"] = persist_data["base64Data"]
                response["modified_time"] = persist_data["modified"]
        else:
            highest_modified = 0
            kv_results = {}
            for key in request_body["keyList"]:
                
                result = self.get_keyed_data(persist_req_data, key)
                kv_results[result["key_name"]] = result["key_value"].decode('utf8')
                if result["modified"] > highest_modified:
                    highest_modified = result["modified"]
            response["modified"] = highest_modified
            response["keyList"] = kv_results
            response["success"] = True
        return response

    def run(self, env, start_response):
        # the environment variable CONTENT_LENGTH may be empty or missing
        try:
            request_body_size = int(env.get('CONTENT_LENGTH', 0))
        except ValueError:
            request_body_size = 0


        response = {}
        response['success'] = False

        # When the method is POST the variable will be sent
        # in the HTTP request body which is passed by the WSGI server
        # in the file like wsgi.input environment variable.
        request_body = json.loads(env['wsgi.input'].read(request_body_size).decode('utf-8'))

        start_response('200 OK', [('Content-Type','application/json')])

        type_table = {
            "newgame": self.handle_new_game,
            "updategame": self.handle_update_game,
            "set_persist_data": self.handle_set_data,
            "get_persist_data": self.handle_get_data
        }
        try:
            if 'mode' in request_body:
                req_type = request_body["mode"]
                if req_type in type_table:
                    response = type_table[req_type](request_body)
                else:
                    raise OS_InvalidMode()
            else:
                raise OS_InvalidMode()
        except OS_BaseException as e:
            response = e.to_dict()
        #except Exception as error:
            #response = {"error": repr(error)}

        return response
