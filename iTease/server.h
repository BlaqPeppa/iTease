#pragma once
#include <unordered_map>
#include "common.h"
#include "event.h"

namespace iTease {
	class Application;

	enum class ServerMethod : int {
		GET, POST
	};

	struct Session {
		const string id;
		shared_ptr<void> data;
	};

	struct ServerRequest {
		using HeaderMap = unordered_map<string, string>;
		using ParamMap = unordered_map<string, string>;
		using CookieMap = unordered_map<string, string>;
		using PostDataMap = ordered_map<string, string>;

		ServerRequest() = default;
		ServerRequest(string uri, string method = "GET");

		auto header(string) const->string;		// returns empty string on failure

		string uri;
		string method;
		HeaderMap headers;
		ParamMap params;
		CookieMap cookies;
		PostDataMap post_data;
		Session* session;
		//String version = "HTTP/1.1";
	};

	struct ServerResponse {
	public:
		ServerResponse() = default;
		ServerResponse(string content, int status = 200);

		string content;
		string content_type;
		int status = 501;
	};

	using ServerResponsePtr = shared_ptr<ServerResponse>;

	class Server {
	public:
		using OnRequestEvent = const Event<const ServerRequest&, ServerResponse&>;

		Server(Application& app);
		~Server();
		auto connect(int port)->bool;
		auto run()->void;
		auto send_request(const ServerRequest&)->ServerResponsePtr;
		auto create_session()->Session&;
		auto get_session(const string& name)->Session*;
		auto& session() { return *m_session; }
		auto& session() const { return *m_session; }

		static auto http_handler(void* cls, struct MHD_Connection* con, const char* uri, const char* method, const char* ver, const char* upload, size_t* upload_size, void** ptr)->int;
		static auto http_finish(void* cls, struct MHD_Connection* con, void** ptr, enum MHD_RequestTerminationCode tc)->void;

		pair<int, string> redirection;
		map<string, string> set_cookies;

	private:
		Application& m_app;
		void* m_daemon = nullptr;
		size_t m_numConnections = 0;
		vector<struct ServerConnection> m_connections;
		std::unordered_map<string, Session> m_sessions;
		Session* m_session = nullptr;
	};
}