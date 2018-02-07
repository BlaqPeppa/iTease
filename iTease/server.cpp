#include "stdinc.h"
#include <sstream>
#include <libmicrohttpd/microhttpd.h>
#include "server.h"
#include "application.h"
#include "logging.h"

using namespace iTease;

const char* msg_server_busy = "Too many requests, server busy";
constexpr int MaxPostSize = 8 * 1024 * 1024; // 8MB
constexpr int PostBufferSize = 1024;
constexpr int ConnectionTypeGET = 0;
constexpr int ConnectionTypePOST = 1;

namespace iTease {
	struct ServerConnection {
		MHD_PostProcessor* processor = nullptr;
		ServerRequest request;
		ServerMethod method;
		size_t input_size = 0;

		auto add_post_data(const char* key, const char* filename, const char* content_type, const char* transfer_encoding, const char* data, size_t size)->bool;
		auto append_post_data(const char* key, const char* data, size_t size)->bool;
	};
}

bool ServerConnection::add_post_data(const char* key, const char* filename, const char* content_type, const char* transfer_encoding, const char* data, size_t size) {
	if (filename) {
		// todo: implement temporary file manager
		/*std::unique_ptr<FileInfo> file_info{
			new FileInfo{key, filename, content_type ? content_type : "",
			transfer_encoding ? transfer_encoding : ""}};
		file_info->temp_file_name = GetTempFileManager()->CreateTempFileName(id_);
		file_info->data_stream = brillo::FileStream::Open(
			file_info->temp_file_name, brillo::Stream::AccessMode::READ_WRITE,
			brillo::FileStream::Disposition::CREATE_ALWAYS, nullptr);
		if (!file_info->data_stream ||
			!file_info->data_stream->WriteAllBlocking(data, size, nullptr)) {
			return false;
		}
		file_info_.push_back(std::move(file_info));
		last_posted_data_was_file_ = true;*/
		return true;
	}
	string value{data, size};
	input_size += size;
	request.post_data.emplace(key, std::move(value));
	//last_posted_data_was_file_ = false;
	return true;
}

bool ServerConnection::append_post_data(const char* key, const char* data, size_t size) {
	/*if (last_posted_data_was_file_) {
		CHECK(!file_info_.empty());
		CHECK(file_info_.back()->field_name == key);
		FileInfo* file_info = file_info_.back().get();
		return file_info->data_stream->WriteAllBlocking(data, size, nullptr);
	}*/

	(--request.post_data.end()).value().append(data, size);
	input_size += size;
	return true;
}

auto iterate_post(void* coninfo, MHD_ValueKind kind, const char* key, const char* filename, const char* content_type, const char* transfer_encoding, const char* data, uint64_t off, size_t size)->int {
	ServerConnection* con = static_cast<ServerConnection*>(coninfo);

	if ((con->input_size + size) > MaxPostSize)
		return MHD_NO;
	if (off > 0)
		return con->append_post_data(key, data, size);

	return con->add_post_data(key, filename, content_type, transfer_encoding, data, size);
}

static auto send_response(MHD_Connection* con, const void* data, size_t data_size, int status_code)->int {
	int ret;
	MHD_Response *response;

	response = MHD_create_response_from_buffer(data_size, const_cast<void*>(data), MHD_RESPMEM_PERSISTENT);
	if (!response) return MHD_NO;
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
	ret = MHD_queue_response(con, status_code, response);
	MHD_destroy_response(response);
	return ret;
}
auto Server::http_finish(void* cls, MHD_Connection* con, void** ptr, MHD_RequestTerminationCode tc)->void {
	Server* server = reinterpret_cast<Server*>(cls);
	auto con_info = static_cast<ServerConnection*>(*ptr);
	if (!con_info)
		return;

	/*if (con_info->method == ServerMethod::POST) {
		if (con_info->processor) {
			MHD_destroy_post_processor(con_info->processor);
			--server->m_numConnections;
		}

		con_info->input_size = 0;
	}*/

	*ptr = nullptr;

	if (tc == MHD_RequestTerminationCode::MHD_REQUEST_TERMINATED_COMPLETED_OK) {

	}
}
auto Server::http_handler(void* cls, MHD_Connection* con, const char* uri, const char* method, const char* ver, const char* upload, size_t* upload_size, void** ptr)->int {
	Server* server = reinterpret_cast<Server*>(cls);

	if (*ptr == nullptr) {
		auto& pr = server->m_connections.emplace_back();

		// handle too many connections by blocking additional connections
		if (server->m_numConnections >= server->m_connections.size())
			return send_response(con, msg_server_busy, sizeof(*msg_server_busy), MHD_HTTP_SERVICE_UNAVAILABLE);

		auto& con_info = server->m_connections[server->m_numConnections++];
		con_info = ServerConnection();
		*ptr = &con_info;

		con_info.input_size = 0;
		con_info.method = std::equal(method, method + 5, "POST") ? ServerMethod::POST : ServerMethod::GET;
		con_info.request = ServerRequest(uri, method);

		if (con_info.method == ServerMethod::POST) {
			con_info.processor = MHD_create_post_processor(con, PostBufferSize, &iterate_post, &con_info);
			if (!con_info.processor) {
				--server->m_numConnections;
				return MHD_NO;
			}
		}
		else con_info.method = ServerMethod::GET;
		return MHD_YES;
	}

	auto con_info = static_cast<ServerConnection*>(*ptr);
	string post_str;
	//*ptr = nullptr;
	auto& request = con_info->request;

	if (con_info->method == ServerMethod::POST) {
		if (*upload_size != 0) {
			auto rc = MHD_post_process(con_info->processor, upload, *upload_size);
			*upload_size = 0;
			return MHD_YES;
		}

		if (con_info->processor) {
			MHD_destroy_post_processor(con_info->processor);
			--server->m_numConnections;
		}

		con_info->input_size = 0;
	}

	ITEASE_LOGDEBUG("HTTP " << method << " Request at " << uri);

	ServerRequest::HeaderMap headers;
	ServerRequest::ParamMap params;
	ServerRequest::CookieMap cookies;
	ServerRequest::PostDataMap postData;

	MHD_get_connection_values(con, MHD_ValueKind::MHD_HEADER_KIND, [](void* cls, MHD_ValueKind kind, const char* key, const char* val)->int {
		if (key && val)
			reinterpret_cast<ServerRequest::HeaderMap*>(cls)->emplace(key, val);
		return MHD_YES;
	}, &request.headers);
	MHD_get_connection_values(con, MHD_ValueKind::MHD_GET_ARGUMENT_KIND, [](void* cls, MHD_ValueKind kind, const char* key, const char* val)->int {
		if (key && val)
			reinterpret_cast<ServerRequest::ParamMap*>(cls)->emplace(key, val);
		return MHD_YES;
	}, &request.params);
	MHD_get_connection_values(con, MHD_ValueKind::MHD_COOKIE_KIND, [](void* cls, MHD_ValueKind kind, const char* key, const char* val)->int {
		if (key && val)
			reinterpret_cast<ServerRequest::CookieMap*>(cls)->emplace(key, val);
		return MHD_YES;
	}, &request.cookies);
	MHD_get_connection_values(con, MHD_ValueKind::MHD_POSTDATA_KIND, [](void* cls, MHD_ValueKind kind, const char* key, const char* val)->int {
		if (key && val)
			reinterpret_cast<ServerRequest::PostDataMap*>(cls)->emplace(key, val);
		return MHD_YES;
	}, &request.post_data);

	server->set_cookies.clear();
	server->redirection.first = 0;
	server->redirection.second = "";

	// find session or create new one
	Session* session = nullptr;
	auto it = request.cookies.find("session");
	session = it != request.cookies.end() ? server->get_session(it->second) : nullptr;
	if (!session) {
		session = &server->create_session();
		server->set_cookies.emplace("session", session->id);
	}
	request.session = session;

	assert(request.session);

	if (auto response = server->send_request(request)) {
		auto resp = MHD_create_response_from_buffer(response->content.size(), const_cast<char*>(response->content.c_str()), MHD_ResponseMemoryMode::MHD_RESPMEM_MUST_COPY);
		string str;
		str.reserve(400);
		if (!response->content_type.empty())
			MHD_add_response_header(resp, "Content-Type", response->content_type.c_str());
		if (!server->redirection.second.empty()) {
			str = std::to_string(server->redirection.first) + "; url=" + server->redirection.second;
			MHD_add_response_header(resp, "Refresh", str.c_str());
		}
		for (auto& cookie : server->set_cookies) {
			str = cookie.first + "=" + cookie.second;
			MHD_add_response_header(resp, MHD_HTTP_HEADER_SET_COOKIE, str.c_str());
		}
		ITEASE_LOGDEBUG("HTTP Response (" << response->status << ") " << response->content_type << ", size: " << response->content.size());
		auto res = MHD_queue_response(con, response->status, resp);
		MHD_destroy_response(resp);
		return res;
	}
	ITEASE_LOGDEBUG("No response");
	return MHD_NO;
}

ServerRequest::ServerRequest(string uri, string method) : uri(uri), method(method)
{ }
auto ServerRequest::header(string name) const->string {
	auto it = headers.find(name);
	return it != headers.end() ? it->second : "";
}
ServerResponse::ServerResponse(string content, int status) : content(content), status(status)
{ }

Server::Server(Application& app) : m_app(app) {
	ITEASE_LOGINFO("Starting iTease web server on port " << app.opt.port);
	if (!connect(app.opt.port)) {
		ITEASE_LOGERROR("Failed to start iTease web server on port " << app.opt.port);
		throw(std::runtime_error("failed to start HTTP server on port " + std::to_string(app.opt.port)));
	}
}
Server::~Server() {
	MHD_stop_daemon((MHD_Daemon*)m_daemon);
}
auto Server::connect(int port)->bool {
	if (port) {
		m_connections.resize(16);
		m_daemon = MHD_start_daemon(/*MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD*/0
			#ifdef _DEBUG
			| MHD_USE_DEBUG
			#endif
			, port, NULL, NULL, &http_handler, this, MHD_OPTION_NOTIFY_COMPLETED, &http_finish, this, MHD_OPTION_END);
	}
	return !!m_daemon;
}
auto Server::run()->void {
	MHD_run((MHD_Daemon*)m_daemon);
}
auto Server::create_session()->Session& {
	auto sid = rand_md5();
	auto pr = m_sessions.emplace(sid, Session{sid});
	if (!pr.second) throw std::bad_alloc();
	return pr.first->second;
}
auto Server::get_session(const string& name)->Session* {
	auto it = m_sessions.find(name);
	return it != m_sessions.end() ? &it->second : nullptr;
}
auto Server::send_request(const ServerRequest& request)->ServerResponsePtr {
	static auto request404 = std::make_shared<ServerResponse>("404 Not Found", MHD_HTTP_NOT_FOUND);
	auto response = std::make_shared<ServerResponse>("", MHD_HTTP_NOT_FOUND);
	if (m_app.server_request(request, *response)) {
		return response;
	}
	return request404;
}