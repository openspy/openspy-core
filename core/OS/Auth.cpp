#include <OS/OpenSpy.h>
#include <OS/Auth.h>
#include <curl/curl.h>
#include <jansson.h>
#include <string>
#include <OS/Cache/GameCache.h>

namespace OS {
	OS::TaskPool<AuthTask, AuthRequest> *m_auth_task_pool = NULL;
	OS::GameCache *m_game_cache;
	struct curl_data {
	    std::string buffer;
	};
	/* callback for curl fetch */
	size_t AuthTask::curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if (!contents) {
			return 0;
		}
		size_t realsize = size * nmemb;                             /* calculate buffer size */
		curl_data *data = (curl_data *)userp;
		data->buffer += (const char *)contents;
		return realsize;
	}
	void AuthTask::PerformAuth_Uniquenick_GPHash(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object(), *profile_obj = json_object();

		if (request.profile.uniquenick.length())
			json_object_set_new(profile_obj, "uniquenick", json_string(request.profile.uniquenick.c_str()));
		
		json_object_set_new(user_obj, "partnercode", json_integer(request.user.partnercode));
		json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile.namespaceid));
		
		json_object_set_new(send_obj, "hash_type", json_string("gp_uniquenick"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());


		json_object_set_new(send_obj, "server_challenge", json_string(request.server_challenge.c_str()));
		json_object_set_new(send_obj, "client_challenge", json_string(request.client_challenge.c_str()));
		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));

		json_object_set(send_obj, "user", user_obj);
		json_object_set(send_obj, "profile", profile_obj);


		char *json_data_str = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;
		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data_str);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if (res == CURLE_OK) {


				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);

				if (json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if (success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if (profile_json) {
							profile = LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(profile_json, "user");
							if (user_json) {
								user = LoadUserFromJson(user_json);
								success = true;
							}
						}
						json_t *server_response_json = json_object_get(json_data, "server_response");
						if (server_response_json) {
							auth_data.hash_proof = json_string_value(server_response_json);
						}
						server_response_json = json_object_get(json_data, "session_key");
						if (server_response_json) {
							auth_data.session_key = json_string_value(server_response_json);
						}

					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if (reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				}
				else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}

				request.callback(success, user, profile, auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}

		if (json_data_str)
			free((void *)json_data_str);

		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_NickEMail_GPHash(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object(), *profile_obj = json_object();

		if(request.profile.nick.length())
			json_object_set_new(profile_obj, "nick", json_string(request.profile.nick.c_str()));
		if(request.profile.uniquenick.length())
			json_object_set_new(profile_obj, "uniquenick", json_string(request.profile.uniquenick.c_str()));

		if(request.user.email.length())
			json_object_set_new(user_obj, "email", json_string(request.user.email.c_str()));

		json_object_set_new(user_obj, "partnercode", json_integer(request.user.partnercode));
		json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile.namespaceid));
		
		if(request.user.password.length())
			json_object_set_new(user_obj, "password", json_string(request.user.password.c_str()));

		json_object_set_new(send_obj, "hash_type", json_string("gp_nick_email"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());


		json_object_set_new(send_obj, "server_challenge", json_string(request.server_challenge.c_str()));
		json_object_set_new(send_obj, "client_challenge", json_string(request.client_challenge.c_str()));
		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));

		json_object_set(send_obj, "user", user_obj);
		json_object_set(send_obj, "profile", profile_obj);


		char *json_data_str = json_dumps(send_obj, 0);
		
		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data_str);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			bool success = false;			

			if(res == CURLE_OK) {
				

				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);

				if(json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if(success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if(profile_json) {
							profile = LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(profile_json, "user");
							if(user_json) {
								user = LoadUserFromJson(user_json);
								success = true;
							}
						}
						json_t *server_response_json = json_object_get(json_data, "server_response");
						if(server_response_json) {
							auth_data.hash_proof = json_string_value(server_response_json);
						}
						server_response_json = json_object_get(json_data, "session_key");
						if(server_response_json) {
							auth_data.session_key = json_string_value(server_response_json);
						}

					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if(reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				} else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}

				request.callback(success, user, profile, auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}

		if(json_data_str)
			free((void *)json_data_str);

		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_PreAuth_Token(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object(), *profile_obj = json_object();

		if (request.profile.nick.length())
			json_object_set_new(profile_obj, "nick", json_string(request.profile.nick.c_str()));
		if (request.profile.uniquenick.length())
			json_object_set_new(profile_obj, "uniquenick", json_string(request.profile.uniquenick.c_str()));

		if (request.user.email.length())
			json_object_set_new(user_obj, "email", json_string(request.user.email.c_str()));

		if (request.user.password.length())
			json_object_set_new(send_obj, "auth_token", json_string(request.user.password.c_str()));

		json_object_set_new(send_obj, "server_challenge", json_string(request.server_challenge.c_str()));
		json_object_set_new(send_obj, "client_challenge", json_string(request.client_challenge.c_str()));
		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));

		json_object_set_new(send_obj, "hash_type", json_string("gp_preauth"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());

		json_object_set(send_obj, "user", user_obj);
		json_object_set(send_obj, "profile", profile_obj);

		char *json_data = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;
		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if (res == CURLE_OK) {

				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);


				if (json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if (success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if (profile_json) {
							profile = LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(profile_json, "user");
							if (user_json) {
								user = LoadUserFromJson(user_json);
								success = true;
							}
						}
						json_t *server_response_json = json_object_get(json_data, "session_key");
						if (server_response_json) {
							auth_data.session_key = json_string_value(server_response_json);
						}
						server_response_json = json_object_get(json_data, "server_response");
						if (server_response_json) {
							auth_data.hash_proof = json_string_value(server_response_json);
						}

					}
					else {
					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if (reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				}
				else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}
				request.callback(success, user, profile, auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}
		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_NickEMail(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object(), *profile_obj = json_object();

		if(request.profile.nick.length())
			json_object_set_new(profile_obj, "nick", json_string(request.profile.nick.c_str()));
		if(request.profile.uniquenick.length())
			json_object_set_new(profile_obj, "uniquenick", json_string(request.profile.uniquenick.c_str()));

		if(request.user.email.length())
			json_object_set_new(user_obj, "email", json_string(request.user.email.c_str()));

		json_object_set_new(user_obj, "partnercode", json_integer(request.user.partnercode));
		json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile.namespaceid));
		
		if(request.user.password.length())
			json_object_set_new(user_obj, "password", json_string(request.user.password.c_str()));

		json_object_set_new(send_obj, "hash_type", json_string("nick_email"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());

		json_object_set(send_obj, "user", user_obj);
		json_object_set(send_obj, "profile", profile_obj);


		char *json_data = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if(res == CURLE_OK) {

				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);


				if(json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if(success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if(profile_json) {
							profile = LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(profile_json, "user");
							if(user_json) {
								user = LoadUserFromJson(user_json);
								success = true;
							}
						}
						json_t *server_response_json = json_object_get(json_data, "session_key");
						if(server_response_json) {
							auth_data.session_key = json_string_value(server_response_json);
						}
					} else {
					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if(reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				} else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}
				request.callback(success, user, profile, auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}
		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_CreateUser_OrProfile(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object(), *profile_obj = json_object();

		if(request.profile.nick.length())
			json_object_set_new(profile_obj, "nick", json_string(request.profile.nick.c_str()));
		if(request.profile.uniquenick.length())
			json_object_set_new(profile_obj, "uniquenick", json_string(request.profile.uniquenick.c_str()));

		if(request.user.email.length())
			json_object_set_new(user_obj, "email", json_string(request.user.email.c_str()));

		json_object_set_new(user_obj, "partnercode", json_integer(request.user.partnercode));
		json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile.namespaceid));

		if(request.user.password.length())
			json_object_set_new(user_obj, "password", json_string(request.user.password.c_str()));

		if (request.profile.birthday.GetYear() != 0) {
			json_object_set_new(profile_obj, "birthday", request.profile.birthday.GetJson());
		}


		json_object_set_new(send_obj, "hash_type", json_string("auth_or_create_profile"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());

		json_object_set(send_obj, "user", user_obj);
		json_object_set(send_obj, "profile", profile_obj);



		char *json_data = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		if (request.gamename.length() > 0) {
			auth_data.gamedata = OS::GetGameByName(request.gamename.c_str());
		}

		auth_data.response_code = (AuthResponseCode)-1;
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			if (request.gamename.length() > 0) {
				auth_data.gamedata = OS::GetGameByName(request.gamename.c_str());
			}

			bool success = false;

			auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;

			if(res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if(json_data) {
					json_t *reason_json = json_object_get(json_data, "reason");
					if(reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					} else {
						json_t *profile_json = json_object_get(json_data, "profile");
						json_t *user_json = json_object_get(profile_json, "user");
						user = LoadUserFromJson(user_json);
						profile = LoadProfileFromJson(profile_json);
						
						user_json = json_object_get(json_data, "new_profile"); //new account created or not
						success = json_boolean_value(user_json);
					}
					json_decref(json_data);
				}
			}
			request.callback(success, user, profile, auth_data, request.extra, request.operation_id, request.peer);
			curl_easy_cleanup(curl);
		}
		
		if(json_data)
			free(json_data);
		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_MakeAuthTicket(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object();

		json_object_set_new(send_obj, "profileid", json_integer(request.profile.id));

		if (request.user.password.length())
			json_object_set_new(send_obj, "password", json_string(request.user.password.c_str()));

		json_object_set_new(send_obj, "mode", json_string("make_auth_ticket"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());

		json_object_set_new(send_obj, "session_key", json_integer(request.session_key));

		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;


		if (curl) {

			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_dump);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if (success_obj == json_true()) {
						success = true;
						json_t *session_key_json = json_object_get(json_data, "ticket");
						if (session_key_json) {
							auth_data.session_key = json_string_value(session_key_json);
						}

						session_key_json = json_object_get(json_data, "challenge");
						if (session_key_json) {
							auth_data.hash_proof = json_string_value(session_key_json);
						}
					}
					else {
					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if (reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				}
				else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}
				request.callback(success, OS::User(), OS::Profile(), auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}
		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_PID_GSStats_SessKey(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object();

		json_object_set_new(send_obj, "profileid", json_integer(request.profile.id));
		
		json_object_set_new(send_obj, "hash_type", json_string("gstats_pid_sesskey"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());


		json_object_set_new(send_obj, "client_response", json_string(request.client_response.c_str()));
		json_object_set_new(send_obj, "session_key", json_integer(request.session_key));


		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;


		if(curl) {

			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_dump);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if(res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if(json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if(success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if(profile_json) {
							profile = LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(profile_json, "user");
							if(user_json) {
								user = LoadUserFromJson(user_json);
								success = true;
							}
						}
						json_t *session_key_json = json_object_get(json_data, "session_key");
						if(session_key_json) {
							auth_data.session_key = json_string_value(session_key_json);
						}

					} else {
					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if(reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				} else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}

				request.callback(success, user, profile, auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}
		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_EmailPass(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object();

		json_object_set_new(user_obj, "email", json_string(request.user.email.c_str()));
		json_object_set_new(user_obj, "partnercode", json_integer(request.user.partnercode));

		if (request.user.password.length())
			json_object_set_new(user_obj, "password", json_string(request.user.password.c_str()));

		json_object_set_new(send_obj, "hash_type", json_string("plain"));
		json_object_set_new(send_obj, "set_context", json_string("user"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());

		json_object_set_new(send_obj, "session_key", json_integer(request.session_key));

		json_object_set(send_obj, "user", user_obj);

		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;


		if (curl) {

			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_dump);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if (success_obj == json_true()) {
						json_t *user_json = json_object_get(json_data, "user");
						if (user_json) {
							user = LoadUserFromJson(user_json);
							success = true;
						}
						json_t *session_key_json = json_object_get(json_data, "session_key");
						if (session_key_json) {
							auth_data.session_key = json_string_value(session_key_json);
						}
					}
					else {
					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if (reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				}
				else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}

				request.callback(success, user, OS::Profile(), auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}
		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);
	}
	void AuthTask::PerformAuth_Uniquenick_Password(AuthRequest request) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *user_obj = json_object(), *profile_obj = json_object();

		json_object_set_new(profile_obj, "uniquenick", json_string(request.profile.uniquenick.c_str()));

		if (request.user.password.length())
			json_object_set_new(user_obj, "password", json_string(request.user.password.c_str()));

		if (request.user.partnercode != -1)
			json_object_set_new(user_obj, "partnercode", json_integer(request.user.partnercode));

		if (request.profile.namespaceid != -1)
			json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile.namespaceid));

		json_object_set_new(send_obj, "hash_type", json_string("plain"));
		json_object_set_new(send_obj, "set_context", json_string("profile"));
		json_object_set_new(send_obj, "save_session", request.create_session ? json_true() : json_false());

		json_object_set_new(send_obj, "session_key", json_integer(request.session_key));

		json_object_set(send_obj, "user", user_obj);
		json_object_set(send_obj, "profile", profile_obj);


		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		Profile profile;
		User user;
		user.id = 0;
		profile.id = 0;
		OS::AuthData auth_data;

		auth_data.response_code = (AuthResponseCode)-1;


		if (curl) {

			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_AUTH_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_dump);

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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (json_data) {
					json_t *success_obj = json_object_get(json_data, "success");
					if (success_obj == json_true()) {
						json_t *profile_json = json_object_get(json_data, "profile");
						if (profile_json) {
							profile = LoadProfileFromJson(profile_json);
							json_t *user_json = json_object_get(profile_json, "user");
							if (user_json) {
								user = LoadUserFromJson(user_json);
								success = true;
							}
						}
						json_t *session_key_json = json_object_get(json_data, "session_key");
						if (session_key_json) {
							auth_data.session_key = json_string_value(session_key_json);
						}

					}
					else {
					}
					json_t *reason_json = json_object_get(json_data, "reason");
					if (reason_json) {
						auth_data.response_code = (AuthResponseCode)json_integer_value(reason_json);
					}
					json_decref(json_data);
				} else {
					success = false;
					auth_data.response_code = LOGIN_RESPONSE_SERVER_ERROR;
				}

				request.callback(success, user, profile, auth_data, request.extra, request.operation_id, request.peer);
			}
			curl_easy_cleanup(curl);
		}

		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);
	}
	void AuthTask::TryMakeAuthTicket(int profileid, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_MakeAuthTicket;
		request.profile.id = profileid;
		request.session_key = 0;
		request.extra = extra;
		request.callback = cb;
		request.create_session = true;
		request.operation_id = operation_id;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryAuthUniqueNick_Plain(std::string uniquenick, int partnercode, int namespaceid, std::string password, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_Uniquenick_Password;
		request.profile.uniquenick = uniquenick;
		request.user.password = password;
		request.session_key = 0;
		request.profile.namespaceid = namespaceid;
		request.user.partnercode = partnercode;
		request.extra = extra;
		request.callback = cb;
		request.create_session = true;
		request.operation_id = operation_id;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryAuthEmailPassword(std::string email, int partnercode, std::string password, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_User_EmailPassword;
		request.user.email = email;
		request.user.password = password;
		request.user.partnercode = partnercode;
		request.extra = extra;
		request.callback = cb;
		request.create_session = true;
		request.operation_id = operation_id;
		request.profile.namespaceid = -1;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}

	void AuthTask::TryAuthTicket(const char *auth_token, const char *server_challenge, const char *client_challenge, const char *response, AuthCallback cb, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_PreAuth_Token;
		request.user.password = auth_token;
		request.server_challenge = server_challenge;
		request.client_challenge = client_challenge;
		request.client_response = response;
		request.callback = cb;
		request.create_session = true;
		request.operation_id = operation_id;
		request.profile.namespaceid = -1;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryAuthUniqueNick_GPHash(std::string uniquenick, int partnercode, int namespaceid, std::string server_chal, std::string client_chal, std::string client_response, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_Uniquenick_GPHash;
		request.profile.uniquenick = uniquenick;
		request.user.partnercode = partnercode;
		request.server_challenge = server_chal;
		request.client_challenge = client_chal;
		request.client_response = client_response;
		request.extra = extra;
		request.callback = cb;
		request.create_session = true;
		request.operation_id = operation_id;
		request.profile.namespaceid = namespaceid;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryAuthNickEmail_GPHash(std::string nick, std::string email, int partnercode, int namespaceid, std::string server_chal, std::string client_chal, std::string client_response, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_NickEmail_GPHash;
		request.user.email = email;
		request.profile.nick = nick;
		request.user.partnercode = partnercode;
		request.server_challenge = server_chal;
		request.client_challenge = client_chal;
		request.client_response = client_response;
		request.extra = extra;
		request.callback = cb;
		request.create_session = true;
		request.operation_id = operation_id;
		request.profile.namespaceid = namespaceid;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryAuthNickEmail(std::string nick, std::string email, int partnercode, std::string pass, bool make_session, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_NickEmail;
		request.user.email = email;
		request.profile.nick = nick;
		request.user.partnercode = partnercode;
		request.extra = extra;
		request.callback = cb;
		request.user.password = pass;
		request.create_session = make_session;
		request.operation_id = operation_id;
		request.profile.namespaceid = -1;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryCreateUser_OrProfile(OS::User user, OS::Profile profile, bool create_session, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_CreateUser_OrProfile;
		request.user = user;
		request.profile = profile;
		request.extra = extra;
		request.callback = cb;
		request.create_session = create_session;
		request.operation_id = operation_id;
		request.profile.namespaceid = -1;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryCreateUser_OrProfile(std::string nick, std::string uniquenick, int namespaceid, std::string email, int partnercode, std::string password, bool create_session, AuthCallback cb, void *extra, int operation_id, INetPeer *peer, std::string gamename) {
		AuthRequest request;
		request.type = EAuthType_CreateUser_OrProfile;
		request.user.email = email;
		request.profile.nick = nick;
		request.profile.uniquenick = uniquenick;
		request.user.partnercode = partnercode;
		request.profile.namespaceid = namespaceid;
		request.extra = extra;
		request.callback = cb;
		request.user.password = password;
		request.create_session = create_session;
		request.operation_id = operation_id;
		request.gamename = gamename;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	void AuthTask::TryAuthPID_GStatsSessKey(int profileid, int session_key, std::string response, AuthCallback cb, void *extra, int operation_id, INetPeer *peer) {
		AuthRequest request;
		request.type = EAuthType_PID_GStats_Sesskey;

		request.profile.id = profileid;
		request.session_key = session_key;
		request.client_response = response;

		request.extra = extra;
		request.operation_id = operation_id;
		request.callback = cb;
		request.create_session = true;
		request.profile.namespaceid = -1;
		request.user.partnercode = 0;
		if (peer) {
			peer->IncRef();
		}
		request.peer = peer;
		m_auth_task_pool->AddRequest(request);
	}
	AuthTask::AuthTask(int thread_index) {
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(AuthTask::TaskThread, this, true);
	}
	AuthTask::~AuthTask() {
		delete mp_mutex;
		delete mp_thread;
	}
	void *AuthTask::TaskThread(CThread *thread) {
		AuthTask *task = (AuthTask *)thread->getParams();
		while (!task->m_request_list.empty() || task->mp_thread_poller->wait()) {
			task->mp_mutex->lock();
			while (!task->m_request_list.empty()) {
				AuthRequest task_params = task->m_request_list.front();
				task->mp_mutex->unlock();
				switch (task_params.type) {
				case EAuthType_NickEmail_GPHash:
					task->PerformAuth_NickEMail_GPHash(task_params);
					break;
				case EAuthType_Uniquenick_GPHash:
					task->PerformAuth_Uniquenick_GPHash(task_params);
					break;
				case EAuthType_Uniquenick_Password:
					task->PerformAuth_Uniquenick_Password(task_params);
					break;
				case EAuthType_User_EmailPassword:
					task->PerformAuth_EmailPass(task_params);
				case EAuthType_NickEmail:
					task->PerformAuth_NickEMail(task_params);
					break;
				case EAuthType_PreAuth_Token:
					task->PerformAuth_PreAuth_Token(task_params);
					break;
				case EAuthType_CreateUser_OrProfile:
					task->PerformAuth_CreateUser_OrProfile(task_params);
					break;
				case EAuthType_PID_GStats_Sesskey:
					task->PerformAuth_PID_GSStats_SessKey(task_params);
					break;
				case EAuthType_MakeAuthTicket:
					task->PerformAuth_MakeAuthTicket(task_params);
					break;
				}

				task->mp_mutex->lock();
				if(task_params.peer)
					task_params.peer->DecRef();
				task->m_request_list.pop();
			}

			task->mp_mutex->unlock();
		}
		return NULL;
	}
	void SetupAuthTaskPool(int num_tasks) {
		OS::DataCacheTimeout gameCacheTimeout;
		gameCacheTimeout.max_keys = 50;
		gameCacheTimeout.timeout_time_secs = 7200;
		m_game_cache = new OS::GameCache(num_tasks + 1, gameCacheTimeout);

		m_auth_task_pool = new OS::TaskPool<AuthTask, AuthRequest>(num_tasks);
	}
	void ShutdownAuthTaskPool() {
		delete m_auth_task_pool;
		delete m_game_cache;
	}
}