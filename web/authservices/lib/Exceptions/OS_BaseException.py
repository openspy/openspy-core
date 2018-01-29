class OS_BaseException(Exception):
    def __init__(self, message):
        print("BaseException: {}\n".format(message))
        self.message = message
        super(OS_BaseException, self).__init__(message)
    def to_json(self):
        return {"message": self.message}