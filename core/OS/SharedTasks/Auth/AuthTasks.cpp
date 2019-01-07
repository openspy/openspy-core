#include <OS/SharedTasks/tasks.h>
#include "AuthTasks.h"
namespace TaskShared {
        TaskScheduler<AuthRequest, TaskThreadData> *InitAuthTasks(INetServer *server) {
            TaskScheduler<AuthRequest, TaskThreadData> *scheduler = new TaskScheduler<AuthRequest, TaskThreadData>(4, server);
            scheduler->AddRequestHandler(EAuthType_User_EmailPassword, PerformAuth_Email_Password);
            scheduler->AddRequestHandler(EAuthType_Uniquenick_Password, PerformAuth_UniqueNick_Password);
            scheduler->AddRequestHandler(EAuthType_MakeAuthTicket, PerformAuth_MakeAuthTicket);
            scheduler->AddRequestHandler(EAuthType_NickEmail, PerformAuth_NickEmail);
			
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
			data->buffer += (const char *)contents;
			return realsize;
		}
		void AuthReq_InitCurl(void *curl, char *post_data, void *write_data, AuthRequest request) {
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

		}
		void Handle_AuthWebError(TaskShared::AuthData &data, json_t *error_obj) {
			std::string error_class, error_name, param_name;
			json_t *item = json_object_get(error_obj, "class");
			if (!item) goto end_error;
			error_class = json_string_value(item);

			item = json_object_get(error_obj, "name");
			if (!item) goto end_error;
			error_name = json_string_value(item);

			item = json_object_get(error_obj, "param");
			if (item) {
				param_name = json_string_value(item);
			}

			if (error_class.compare("common") == 0) {
				if (error_name.compare("MissingParam") == 0 || error_name.compare("InvalidMode") == 0) {
					data.response_code = TaskShared::LOGIN_RESPONSE_SERVER_ERROR;
				}
				if (error_name.compare("InvalidParam") == 0) {
					if (param_name.compare("email") == 0) {
						data.response_code = TaskShared::CREATE_RESPONSE_INVALID_EMAIL;
					}
					else if (param_name.compare("password") == 0) {
						data.response_code = TaskShared::LOGIN_RESPONSE_INVALID_PASSWORD;
					}
				}
			}
			else if (error_class.compare("auth") == 0) {
				if (error_name.compare("InvalidCredentials") == 0) {
					data.response_code = TaskShared::LOGIN_RESPONSE_INVALID_PASSWORD;
				}
				else if (error_name.compare("NoSuchUser") == 0) {
					data.response_code = TaskShared::LOGIN_RESPONSE_USER_NOT_FOUND;
				}
			}
			else if (error_class.compare("profile") == 0) {
				if (error_name.compare("UniqueNickInUse") == 0) {
					data.response_code = TaskShared::LOGIN_RESPONSE_INVALID_PROFILE;
				}
				else if (error_name.compare("NickInvalid") == 0) {
					data.response_code = TaskShared::LOGIN_RESPONSE_INVALID_PROFILE;
				}
			}
			return;
		end_error:
			data.response_code = TaskShared::LOGIN_RESPONSE_SERVER_ERROR;

		}
}