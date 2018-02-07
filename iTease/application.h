#pragma once
#include "common.h"
#include "iTease.h"
#include "logging.h"
#include "server.h"
#include "db.h"
#include "webui.h"
#include "js.h"
#include "plugin.h"
#include "module.h"

namespace iTease {
	class WebUI;

	class Options {
	public:
		int			port = 36900;
		LogLevel	logLevel = LogLevel::Warning;
	};

	struct tag_nocontext_t { };
	constexpr tag_nocontext_t nocontext = { };

	template<typename... Targs>
	auto format_log(const Application& app, const char* type, const char* msg, Targs&&... args)->string;
	template<typename... Targs>
	auto format_log(const Application& app, const char* type, const char* msg, const sqlite::sqlite_exception& ex, Targs&&... args)->string;

	extern const char* get_content_type_by_extension(string_view ext);

	class ProgramFailure : public std::exception
	{ };

	class ConfigError : public std::runtime_error {
	public:
		template<typename... Args>
		ConfigError(const char* msg, Args&&... args)
			: std::runtime_error(fmt::format(msg, std::forward<Args>(args)...))
		{}
	};

	class SystemError {
	public:
		SystemError();
		SystemError(int e, string message);

		void create(JS::Context&) const;

	private:
		int m_errno;
		string m_message;
	};

	/**
	 * Application class - Context for the application
	**/
	class Application {
		friend class Server;

	public:
		using OnTickEvent = const Event<long long&>;

	public:
		Options opt;
		unique_ptr<JS::Context> js;
		unique_ptr<Database> db;
		ci_map<string, std::unique_ptr<Module>> modules;

		// fired for server requests not handled internally (via OnServerRequestInternal), first serve, first serve
		Server::OnRequestEvent OnServerRequest;
		// fired before OnServerRequest, if request handled by this, OnServerRequest won't be called
		Server::OnRequestEvent OnServerRequestInternal;

	public:
		Application(string args = "");
		Application(wstring args);
		~Application();

		auto init()->bool;
		auto run()->void;
		auto add_interval_callback(long long intervalMS, OnTickEvent::Handler h)->OnTickEvent::Listener;
		auto is_running() {
			return !m_exit;
		}
		auto exit() {
			m_exit = true;
		}

		template<typename... Targs>
		auto error(const string& msg, Targs&&... args) {
			//ITEASE_LOGERROR(formatted_msg);
			Error() << format_log(*this, "error", msg.c_str(), std::forward<Targs>(args)...);
		}
		template<typename... Targs>
		auto warning(const string& msg, Targs&&... args) {
			Warning() << format_log(*this, "warning", msg.c_str(), std::forward<Targs>(args)...);
		}

		auto& get_args() const { return m_args; }

		template<typename T, typename... Targs>
		auto add_module(Targs&&... args) {
			auto ptr = std::make_unique<T>(std::forward<Targs>(args)...);
			modules.emplace(ptr->name(), std::move(ptr));
		}

	public:
		auto init_js(JS::Context&, bool = false)->void;
		auto partial_restart()->void;

	private:
		struct EventInterval {
			std::chrono::steady_clock::duration interval;
			std::chrono::steady_clock::time_point last_run;
		};

		auto initDB()->bool;
		auto initPlugins()->void;
		auto initScripts()->void;
		auto initModules()->void;
		auto server_request(const ServerRequest&, ServerResponse&)->bool;
		auto parse_args(const vector<string>&)->map<string, string>;

	private:
		vector<pair<EventInterval, unique_ptr<OnTickEvent>>> m_onTicks;
		unique_ptr<WebUI> m_ui;
		vector<unique_ptr<Plugin>> m_plugins;
		map<string, string> m_args;
		bool m_started = false;
		bool m_exit = false;
	};

	template<typename... Targs>
	auto format_log(const Application& app, const char* type, const char* msg, Targs&&... args)->string {
		return fmt::format("Application {}: {}", type, msg);
	}
	template<typename... Targs>
	auto format_log(const Application& app, const char* type, const char* msg, const sqlite::sqlite_exception& ex, Targs&&... args)->string {
		return format_log(app, type, fmt::format("{} : {}\n", msg, ex.what()).c_str(), std::forward<Targs>(args)...);
	}
}