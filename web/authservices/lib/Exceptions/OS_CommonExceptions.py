from lib.Exceptions.OS_BaseException import OS_BaseException
class OS_InvalidMode(OS_BaseException):
    def __init__(self):
        self.data = {}
        self.data["class"] = "common"
        self.data["message"] = "Invalid Mode"
        self.data["name"] = "InvalidMode"
        super(OS_BaseException, self).__init__(self.data["message"])
class OS_MissingParam(OS_BaseException):
    def __init__(self, param_name):
        self.data = {}
        self.data["class"] = "common"
        self.data["name"] = "MissingParam"
        self.data["message"] = "Missing required parameter ({})".format(param_name)
        super(OS_BaseException, self).__init__(self.data["message"])
class OS_InvalidParam(OS_BaseException):
    def __init__(self, param_name):
        self.data = {}
        self.data["class"] = "common"
        self.data["name"] = "InvalidParam"
        self.data["message"] = "Invalid parameter ({})".format(param_name)
        super(OS_BaseException, self).__init__(self.data["message"])

#class OS_UniqueNickInUse(OS_BaseException):
    #def __init__(self, param_name):
        #self.data = {}
        #self.data["class"] = "profile"
        #self.data["name"] = "UniqueNickInUse"
        #self.data["message"] = "Unique nick in use ({})".format(param_name)
        #super(OS_BaseException, self).__init__(self.data["message"])

class OS_UserExists(OS_BaseException):
    def __init__(self):
        self.data = {}
        self.data["class"] = "common"
        self.data["name"] = "UserExists"
        self.data["message"] = "User already exists"
        super(OS_BaseException, self).__init__(self.data["message"])

class OS_Auth_InvalidCredentials(OS_BaseException):
    def __init__(self):
        self.data = {}
        self.data["class"] = "auth"
        self.data["name"] = "InvalidCredentials"
        self.data["message"] = "Invalid Credentials"
        super(OS_BaseException, self).__init__(self.data["message"])

class OS_Auth_NoSuchUser(OS_BaseException): #used with UserProfileMgrService as well
    def __init__(self):
        self.data = {}
        self.data["class"] = "auth"
        self.data["name"] = "NoSuchUser"
        self.data["message"] = "No such user"
        super(OS_BaseException, self).__init__(self.data["message"])

class OS_Profile_NickInvalid(OS_BaseException):
    def __init__(self):
        self.data = {}
        self.data["class"] = "profile"
        self.data["name"] = "NickInvalid"
        self.data["message"] = "Invalid Nickname"
        super(OS_BaseException, self).__init__(self.data["message"])

class OS_Profile_UniquenickInvalid(OS_BaseException):
    def __init__(self):
        self.data = {}
        self.data["class"] = "profile"
        self.data["name"] = "UniqueNickInvalid"
        self.data["message"] = "Invalid Unique Nick"
        super(OS_BaseException, self).__init__(self.data["message"])

class OS_Profile_UniquenickInUse(OS_BaseException):
    def __init__(self, profile):
        self.data = {}
        self.data["class"] = "profile"
        self.data["name"] = "UniqueNickInUse"
        self.data["message"] = "Unique nick in use"
        self.data["profile"] = profile
        super(OS_BaseException, self).__init__(self.data["message"])