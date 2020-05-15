#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>

namespace TaskShared {
		TaskScheduler<AuthRequest, TaskThreadData>::RequestHandlerEntry AuthTask_HandlerTable[] = {
			{EAuthType_User_EmailPassword, PerformAuth_Email_Password},
			{EAuthType_Uniquenick_Password, PerformAuth_UniqueNick_Password},
			{EAuthType_MakeAuthTicket, PerformAuth_MakeAuthTicket},
			{EAuthType_NickEmail, PerformAuth_NickEmail},
			{EAuthType_MakeAuthSession, PerformAuth_MakeAuthSession},
			{EAuthType_DeleteAuthSession, PerformAuth_DeleteAuthSession},
			{NULL, NULL}
		};
        TaskScheduler<AuthRequest, TaskThreadData> *InitAuthTasks(INetServer *server) {
            TaskScheduler<AuthRequest, TaskThreadData> *scheduler = new TaskScheduler<AuthRequest, TaskThreadData>(OS::g_numAsync, server, AuthTask_HandlerTable, NULL);
			scheduler->DeclareReady();
            return scheduler;
        }


		/* callback for curl fetch */
		size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
			if (!contents) {
				return 0;
			}
			size_t realsize = size * nmemb;                             /* calculate buffer size */
			TaskShared::curl_data *data = (curl_data *)userp;
			std::string raw_string = std::string((const char *)contents, realsize);
			std::string input = OS::strip_whitespace(raw_string);
			if(input.length() > 0)
				data->buffer += input.c_str();
			return realsize;
		}
		void AuthReq_InitCurl(void *curl, char *post_data, void *write_data, AuthRequest request, struct curl_slist **out_list) {
			struct curl_slist *chunk = NULL;
			std::string apiKey = "APIKey: " + std::string(OS::g_webServicesAPIKey);
			chunk = curl_slist_append(chunk, apiKey.c_str());
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-App: ") + OS::g_appName).c_str());
			if (request.peer) {
				chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-Peer-Address: ") + request.peer->getAddress().ToString()).c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			std::string url = OS::g_webServicesURL;
			switch (request.type) {
			case EAuthType_MakeAuthTicket:
				url += "/v1/Presence/Auth/GenAuthTicket";
				break;
			case EAuthType_NickEmail:
			case EAuthType_Uniquenick_Password:
			case EAuthType_User_EmailPassword:
				url += "/v1/Auth/Login";
				break;
			case EAuthType_MakeAuthSession:
				url += "/v1/Auth/Session";
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
				break;
			case EAuthType_DeleteAuthSession:
				url += "/v1/Auth/Session";
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
				break;
			default:
				int *x = (int *)NULL;
				*x = 1;
				break;
			}
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSCoreAuth");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			/* Close socket after one use */
			curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TaskShared::curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_data);

			if(out_list != NULL) {
				*out_list = chunk;
			}

		}
		bool Handle_WebError(json_t *json_body, WebErrorDetails &error_info) {
			if (json_body == NULL) {
				error_info.response_code = WebErrorCode_BackendError;
				return true;
			}
			json_t *error_obj = json_object_get(json_body, "error");
			if (!error_obj) {
				error_info.response_code = WebErrorCode_Success;
				return false;
			}
			error_info.userid = 0;
			error_info.profileid = 0;
			
			//{"error":{"class":"profile","name":"UniqueNickInUse","extra":{"profileid":2957553,"userid":1}}}
			std::string error_class, error_name;
			json_t *extra = json_object_get(error_obj, "extra");
			json_t *item = json_object_get(error_obj, "class");
			if (!item) goto end_error;
			error_class = json_string_value(item);

			item = json_object_get(error_obj, "name");
			if (!item) goto end_error;
			error_name = json_string_value(item);

			if(extra) {
				json_t *profileid_obj = json_object_get(extra, "profileid");
				if(profileid_obj) {
					error_info.profileid = json_integer_value(profileid_obj);
				}

				json_t *userid_obj = json_object_get(extra, "userid");
				if(userid_obj) {
					error_info.userid = json_integer_value(userid_obj);
				}

			}

			if (error_class.compare("common") == 0) {
				if (error_name.compare("NoSuchUser") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_NoSuchUser;
				}
			} else if (error_class.compare("auth") == 0) { 
				if (error_name.compare("InvalidCredentials") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_AuthInvalidCredentials;
				} else if (error_name.compare("AccountBanned") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_AuthAccountBanned;
				} else if (error_name.compare("AccountDisabled") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_AuthAccountDisabled;
				}
			} else if (error_class.compare("profile") == 0) {
				if (error_name.compare("CannotDeleteLastProfile") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_CannotDeleteLastProfile;
				}
				else if (error_name.compare("NickInUse") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_NickInUse;
				}
				else if (error_name.compare("NickInvalid") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_NickInvalid;
				}
				else if (error_name.compare("UniqueNickInUse") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_UniqueNickInUse;
				}
				else if (error_name.compare("EmailInUse") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_EmailInUse;
				}
			}
			else if (error_class.compare("cdkey") == 0) {
				if (error_name.compare("AlreadySet") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_CdKeyAlreadySet;
				}
				else if (error_name.compare("InUse") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_CdKeyAlreadyTaken;
				}
				else if (error_name.compare("BadKey") == 0) {
					error_info.response_code = TaskShared::WebErrorCode_BadCdKey;
				}
			}

			return true;
			end_error:
			return false;
		}
}