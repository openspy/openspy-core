from pymongo import MongoClient
from bson import ObjectId
import time
import os
class SimpleSnapshotHandler():
    def __init__(self):
        self.gamestats_db_ctx = MongoClient(os.environ['SNAPSHOT_DB_HOST'], int(os.environ['SNAPSHOT_DB_PORT']))
        self.gamestats_db = self.gamestats_db_ctx.gamestats
    def handle_new_game(self, env, request_body):
        game_record = {"gameid": request_body["game_id"], "profileid": request_body["profileid"], "ip": env["HTTP_X_OPENSPY_PEER_ADDRESS"], "updates": [], "created": time.time()}
        res = self.gamestats_db.snapshots.insert_one(game_record).inserted_id
        snapshot_id = res.inserted_id
        return {"data": {"game_identifier": str(snapshot_id)}, "success": res.inserted_count > 0}
    def handle_update_game(self, env, request_body):
        snapshot_data = {"data": request_body["data"], "created": time.time(), "complete": request_body["complete"], "gameid": request_body["game_id"], "profileid": request_body["profileid"]}
        res = self.gamestats_db.snapshots.update_one({"_id": ObjectId(request_body["game_identifier"])}, {"$push": {"updates": snapshot_data}})
        return {"data": {"game_identifier": request_body["game_identifier"]}, "success": res.modified_count > 0}