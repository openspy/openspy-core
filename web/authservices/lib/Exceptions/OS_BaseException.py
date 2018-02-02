class OS_BaseException(Exception):
    def __init__(self, message):
        print("BaseException: {}\n".format(message))
        super(OS_BaseException, self).__init__(self.data["message"])
    def to_dict(self):
        return self.data