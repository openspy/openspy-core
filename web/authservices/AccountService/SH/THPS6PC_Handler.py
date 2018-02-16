class THPS6PC_Handler():
    def handle_new_game(self, request_body):
        print("THPS6PC New: {}\n".format(request_body))
        return {"thps6pc": "hello"}
    def handle_update_game(self, request_body):
        print("THPS6PC Update: {}\n".format(request_body))
        return {"thps6pc": "update"}