#include "stdinc.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include <curl/curl.h>
#include "iTease.h"
#include "cpp/string.h"
#include "application.h"
#include "db_tables.h"
#include "filesystem.h"
#include "html.h"
#include "network.h"
#include "plugin.h"
#include "server.h"
#include "script.h"
#include "user.h"
#include "webui.h"
#include "web.h"

using namespace iTease;

const Version Version::Latest;

static const map<string, string> application_arguments = {
	{"port", "Port"},
	{"log", "LogLevel"}
};

vector<string> ParseCommandLine(string cmdLine, bool skipFirst = true) {
	auto arguments = vector<string>{};
	unsigned cmdStart = 0, cmdEnd = 0;
	bool inCmd = false;
	bool inQuote = false;

	for (unsigned i = 0; i < cmdLine.length(); ++i) {
		if (cmdLine[i] == '\"')
			inQuote = !inQuote;
		if (cmdLine[i] == ' ' && !inQuote) {
			if (inCmd) {
				inCmd = false;
				cmdEnd = i;
				if (!skipFirst)
					arguments.push_back(cmdLine.substr(cmdStart, cmdEnd - cmdStart));
				skipFirst = false;
			}
		}
		else {
			if (!inCmd) {
				inCmd = true;
				cmdStart = i;
			}
		}
	}
	if (inCmd) {
		cmdEnd = cmdLine.length();
		if (!skipFirst)
			arguments.push_back(cmdLine.substr(cmdStart, cmdEnd - cmdStart));
	}

	// strip double quotes
	for (unsigned i = 0; i < arguments.size(); ++i)
		str_replace(arguments[i], "\"", "");

	return arguments;
}

SystemError::SystemError() : m_errno(errno), m_message(std::strerror(m_errno)) { }
SystemError::SystemError(int e, string msg) : m_errno(e), m_message(std::move(msg)) { }
void SystemError::create(JS::Context& ctx) const {
	ctx.get_global<void>("iTease");
	ctx.get_property<void>(-1, "SystemError");
	ctx.push(m_errno);
	ctx.push(m_message);
	ctx.create(2);
	ctx.remove(-2);
	ctx.remove(-2);
}

Application::Application(string args) {
	m_args = parse_args(ParseCommandLine(args));
	if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
		throw std::runtime_error("curl failed to initialise");
}
Application::Application(wstring args) : Application(to_string(args))
{ }
Application::~Application() {
	modules.clear();
	js.reset();
	curl_global_cleanup();
}
auto Application::init()->bool {
	if (!m_started) {
		if (initDB()) {
			js = std::make_unique<JS::Context>();
			initModules();
			init_js(*js);
			initPlugins();
			m_started = true;
		}
	}
	return m_started;
}
auto Application::run()->void {
	try {
		auto now = std::chrono::steady_clock::now();
		for (auto it = m_onTicks.begin(); it != m_onTicks.end(); ) {
			if ((it->first.last_run + it->first.interval) <= now) {
				long long v = std::chrono::duration_cast<std::chrono::milliseconds>(it->first.interval).count();
				if (!(*it->second)(v))
					it = m_onTicks.erase(it);
				else {
					it->first.interval = std::chrono::nanoseconds(v);
					it->first.last_run = now;
					++it;
				}
			}
			else ++it;
		}
	}
	catch (const JS::ErrorException& ex) {
		ITEASE_LOGERROR(ex.what());
	}
	catch (const JS::Exception& ex) {
		ITEASE_LOGERROR(ex.what());
	}
	catch (const std::exception& ex) {
		ITEASE_LOGERROR(ex.what());
	}
}
auto Application::add_interval_callback(long long intervalMS, OnTickEvent::Handler h)->OnTickEvent::Listener {
	auto ev = std::make_unique<OnTickEvent>();
	auto& pr = m_onTicks.emplace_back(EventInterval{std::chrono::milliseconds{intervalMS}, std::chrono::steady_clock::time_point{ }}, std::move(ev));
	return std::move(pr.second->listen(h));
}
auto Application::initModules()->void {
	add_module<Web>(*this);
	add_module<HTML>();
	add_module<Network>(*this);
	add_module<Users>(*this);
	//add_module<WebUI>(*this);
	add_module<FileSystem>(*this);
	for (auto& m : modules) {
		try {
			m.second->init_module(*this);
		}
		catch (const std::exception& ex) {
			ITEASE_LOGERROR("Failed to initialise module '" << m.first << "': " << ex.what());
		}
	}
}
auto Application::init_js(JS::Context& js, bool omit_scripts)->void {
	JS::StackAssert sa(js);
	js.put_global("print", JS::Function{[](JS::Context& js) {
		std::cout << "JS: ";
		if (js.is<JS::Null>(0))
			std::cout << "[Null]";
		else if (js.is<JS::Undefined>(0))
			std::cout << "[Undefined]";
		else if (js.is<bool>(0))
			std::cout << (js.get<bool>(0) ? "[True]" : "[False]");
		else if (js.is<int>(0)) {
			double d = js.get<double>(0);
			auto i = js.get<int>(0);
			double d2 = i;
			if (d != d2)
				std::cout << d;
			else
				std::cout << i;
		}
		else if (js.is<string>(0))
			std::cout << js.get<string>(0);
		else if (js.is<JS::Object>(0)) {
			std::cout << "[Object]" << std::endl << "{";
			auto vmap = js.get<JS::Object>(0);
			if (!vmap.empty()) {
				std::cout << std::endl;// << js.Encode(0) << std::endl;
				int depth = 0;
				auto indent = [&depth]() {
					std::cout << "\t";
					for (int i = 0; i < depth; ++i) std::cout << "\t";
				};
				auto incIndent = [&depth]() {
					++depth;
				};
				auto decIndent = [&depth]() {
					--depth;
				};
				std::function<void(const JS::VariantMap&)> printMap;
				std::function<void(const JS::Variant&)> printElement;
				std::function<void(JS::VariantVector&)> printVector = [indent, &printElement](const JS::VariantVector& vec) {
					int i = 0;
					auto last = vec.empty() ? vec.end() : --vec.end();
					for (auto it = vec.begin(); it != vec.end(); ++it) {
						indent();
						std::cout << i++ << ": ";
						printElement(*it);
						if (it != last) std::cout << ",";
						std::cout << std::endl;
					}
				};
				printMap = [indent, &printElement](const JS::VariantMap& map) {
					auto last = map.empty() ? map.end() : --map.end();
					for (auto it = map.begin(); it != map.end(); ++it) {
						indent();
						std::cout << it->first << ": ";
						printElement(it->second);
						if (it != last) std::cout << ",";
						std::cout << std::endl;
					}
				};
				printElement = [indent, incIndent, decIndent, printMap, printVector](const JS::Variant& var) {
					auto& type = var.type();
					if (!var.has_value())
						std::cout << "[Null]";
					else if (type == typeid(JS::Function))
						std::cout << "[Function]";
					else if (type == typeid(bool))
						std::cout << (std::any_cast<bool>(var) ? "true" : "false");
					else if (type == typeid(int))
						std::cout << std::any_cast<int>(var);
					else if (type == typeid(double))
						std::cout << std::any_cast<double>(var);
					else if (type == typeid(string))
						std::cout << "\"" << std::any_cast<string>(var) << "\"";
					else if (type == typeid(JS::VariantMap)) {
						std::cout << "{" << std::endl;
						incIndent();
						printMap(std::any_cast<JS::VariantMap>(var));
						decIndent();
						indent();
						std::cout << "}";
					}
					else if (type == typeid(JS::VariantVector)) {
						std::cout << "[" << std::endl;
						incIndent();
						printVector(std::any_cast<JS::VariantVector>(var));
						decIndent();
						indent();
						std::cout << "]";
					}
					else if (type == typeid(vector<string>)) {
						std::cout << "[";
						auto sv = std::any_cast<vector<string>>(var);
						bool b = false;
						for (auto& s : sv) {
							if (b) std::cout << ", ";
							std::cout << "\"" << s << "\"";
							b = true;
						}
						std::cout << "]";
					}
					else if (type == typeid(JS::VarPointer)) {
						auto vp = std::any_cast<JS::VarPointer>(var);
						if (vp.type == JS::PointerType::SharedPointer) {
							auto* shared = reinterpret_cast<shared_ptr<void>*>(vp.ptr);
							std::cout << "[Pointer:0x";
							std::cout.setf(std::ios::hex);
							std::cout << shared->get();
							//std::cout << " (";
							//std::cout.setf(std::ios::dec);
							//std::cout << shared->use_count();		// be wary of compiler bug?
							//std::cout << ")]";
							std::cout << "]";
						}
						else
							std::cout << "[Pointer:0x" << std::ios::hex << vp.ptr << "]";
					}
					else std::cout << "[Unknown]";
				};
				printMap(vmap);
			}
			std::cout << "}" ;
		}
		else if (js.is<JS::Array>(0)) {
			std::cout << "[Array]" << std::endl << "{";
			std::cout << "\tlength: " << js.length(0);
			std::cout << "}";
		}
		else if (js.is<JS::Function>(0)) std::cout << "[Function]" << std::endl << js.encode(0);
		std::cout << std::endl;
		return 0;
	}, 1});
	js.push_object(
		JS::Property<JS::Function>{"resolve", {[](JS::Context& js)->duk_ret_t {
			auto id = js.get<string>(0);
			auto parent = js.get<string>(1);
			Script scr(id, parent);
			js.push(scr.full_path());
			return 1;
		}, DUK_VARARGS}},
		JS::Property<JS::Function>{"load", {[this](JS::Context& js)->duk_ret_t {
			auto id = js.get<string>(0);
			auto it = modules.find(id);
			if (it == modules.end()) {
				JS::StackAssert sa(js, 1);
				Script scr(id);
				scr.read(js);
				return 1;
			}
			else {
				JS::StackAssert sa(js);

				it->second->init_js(js);
				//assert(js.is<JS::Object>(-1));
				js.put_property(2, "exports");
			}
			return 0;
		}, DUK_VARARGS}}
	);
	js.init_module_loader();

	// iTease
	js.put_global_object("iTease",
		JS::Property<JS::Object>{"version", js.push_object(
			JS::Property<int>{"major", ITEASE_VERSION_MAJOR, DUK_DEFPROP_CLEAR_WRITABLE},
			JS::Property<int>{"minor", ITEASE_VERSION_MINOR, DUK_DEFPROP_CLEAR_WRITABLE},
			JS::Property<int>{"patch", ITEASE_VERSION_BETA, DUK_DEFPROP_CLEAR_WRITABLE}
		), DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_CLEAR_WRITABLE},
		JS::Property<JS::Function>{"SystemError", JS::Function{[](JS::Context& js){
			js.push_this_object(
				JS::Property<int>{"errno", js.require<int>(0), DUK_DEFPROP_CLEAR_WRITABLE},
				JS::Property<string>{"message", js.require<string>(1), DUK_DEFPROP_CLEAR_WRITABLE},
				JS::Property<string>{"name", "SystemError", DUK_DEFPROP_CLEAR_WRITABLE}
			);
			js.pop();
			return 0;
		}, 2}, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_CLEAR_WRITABLE}
	);

	// Set prototype for iTease.SystemError
	js.get_global<void>("iTease");
	js.get_property<void>(-1, "SystemError");
	js.get_global<void>("Error");
	js.create(0);
	js.dup(-2);
	js.put_property(-2, "constructor");
	js.put_property(-2, "prototype");
	js.pop(2);

	if (!omit_scripts) initScripts();
}
auto Application::initScripts()->void {
	try {
		JS::StackAssert sa2(*js);
		static vector<string> s_systemScripts = {
			"system/startup"
		};
		for (auto& scr : s_systemScripts) {
			Script script(scr, "system", true);
			script.run(*js);
		}
	}
	catch (const JS::ErrorException& ex) {
		throw ex;
	}
	catch (const JS::ErrorInfo& ex) {
		throw ex;
	}
}
auto Application::partial_restart()->void {
	m_onTicks.clear();
	// be sure to clear modules first as they may need to make JS destruction calls
	modules.clear();
	js.reset();
	js = std::make_unique<JS::Context>();
	initModules();
	init_js(*js);
	initPlugins();
}
auto Application::initDB()->bool {
	try {
		db = std::make_unique<Database>((data_path() / "web.db").string());
		sqlite3_config(SQLITE_CONFIG_LOG, [](void* arg, int errCode, const char* msg) {
			ITEASE_LOGERROR("SQLite: (" << errCode << ") " << msg);
		}, this);
		db->upgrade();
		return true;
	}
	catch (const sqlite::sqlite_exception& ex) {
		if (!ex.get_sql().empty())
			ITEASE_LOGDEBUG("SQL Error : " << ex.get_sql());
		error("database error", ex);
	}
	return false;
}
auto Application::initPlugins()->void {
	auto path = plugin_path();
	for (auto& fp : fs::directory_iterator(path)) {
		if (!fs::is_directory(fp.status()))
			continue;
		auto& dirpath = fp.path();
		auto plugin_pathname = dirpath.filename();
		auto plugin_scriptname = dirpath / plugin_pathname;
		plugin_scriptname += ".js";
		auto plugin_json_path = dirpath / "plugin.json"s;
		if (fs::is_regular_file(plugin_json_path) && fs::is_regular_file(plugin_scriptname)) {
			std::ifstream file(plugin_json_path);
			if (file.is_open()) {
				json pd_json;
				file >> pd_json;
				auto pd = PluginData(pd_json);
				auto plugin = std::make_unique<Plugin>(pd, plugin_scriptname.string());
				init_js(plugin->js, true);
				plugin->init(*this);
				m_plugins.emplace_back(std::move(plugin));
			}
		}
	}
}
auto Application::server_request(const ServerRequest& request, ServerResponse& response)->bool {
	try {
		if (!OnServerRequestInternal(request, response))
			return OnServerRequest(request, response) > 0;
		return true;
	}
	catch (const JS::ErrorException& ex) {
		ITEASE_LOGERROR(ex.what());
	}
	catch (const JS::Exception& ex) {
		ITEASE_LOGERROR(ex.what());
	}
	catch (const JS::ErrorInfo& ex) {
		ITEASE_LOGERROR(ex.fileName << ":" << ex.lineNumber << " " << ex.name << ": " << ex.message << std::endl << ex.stack);
	}
	catch (const std::exception& ex) {
		ITEASE_LOGERROR(ex.what());
	}
	return false;
}
auto Application::parse_args(const vector<string>& args)->map<string, string> {
	map<string, string> parsed_args;
	for (size_t i = 0; i < args.size(); ++i) {
		if (!args[i].empty() && args[i][0] == '-') {
			auto arg = strtolower(args[i].substr(1));
			auto val = i + 1 < args.size() ? args[i + 1] : "";

			auto it = application_arguments.find(arg);
			if (it != application_arguments.end())
				parsed_args[it->first] = val;
		}
	}
	return parsed_args;
}