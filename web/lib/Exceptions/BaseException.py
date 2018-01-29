class BaseException():
    def __init__(self, message):
        print("BaseException: {}\n".format(message))
    def to_json(self):
        return {"error": True}