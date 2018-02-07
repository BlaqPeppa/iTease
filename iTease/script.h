#pragma once
#include <fstream>
#include <sstream>
#include "common.h"
#include "system.h"
#include "logging.h"
#include "js.h"
#include "cpp/string.h"

namespace iTease {
	class ScriptError {
	public:
		ScriptError();
		ScriptError(int, string);
		void create(JS::Context&) const;

	private:
		int m_errno;
		string m_message;
	};

	class Script {
	public:
		Script(string path, string parent = "", bool system = false) : m_originalPath(path), m_parent(fs::path(parent).parent_path().string()), m_path(path), m_system(system) {
			if (system) {
				if (path == "startup" && parent == "system")
					m_entryPoint = true;
				m_path = (root_path() / m_parent / path).string();
			}
			else {
				if (str_begins(path, '/') || str_begins(path, "./") || str_begins(path, "../")) {
					auto parent_path = fs::path(m_parent);
					m_path = fs::canonical(parent_path.is_absolute() ? (parent_path / path) : (root_path() / "system" / fs::path(m_parent) / path), "").string();
				}
			}
		}

		inline auto& path() const { return m_path; }
		string full_path() const {
			auto bpath = fs::path(m_path).make_preferred().string();
			auto path = bpath;
			
			if (!fs::is_regular_file(path)) {
				for (int i = 3; i; --i) {
					if (!m_system && fs::path(bpath).is_relative()) {
						if (i == 3) path = (fs::path(m_parent) / bpath).make_preferred().string();
						else if (i == 2) path = fs::path("system/" + bpath).make_preferred().string();
						else if (i == 1) path = fs::path("plugins/" + bpath).make_preferred().string();
					}
					if (!fs::is_regular_file(path)) {
						if (fs::is_regular_file(path + ".js")) path += ".js";
						else if (fs::is_regular_file(path + ".json")) path += ".json";
						else continue;
					}
					return fs::path(path).make_preferred().string();
				}
				return m_path;
			}
			return fs::path(path).make_preferred().string();
		}

		bool run(JS::Context& js) {
			JS::StackAssert sa(js);
			auto path = full_path();
			if (!path.empty()) {
				std::ifstream file(path);
				if (file.is_open()) {
					std::stringstream ss;
					ss << file.rdbuf();
					try {
						js.peval<void>(ss.str());
					}
					catch (const JS::ErrorException& ex) {
						ITEASE_LOGERROR(ex.what());
						//throw ex;
					}
					catch (...) {
						throw;
					}
				}
			}
			return false;
			//else throw exception(("script '"+m_path+"' not found").c_str());
		}

		bool read(JS::Context& js) {
			JS::StackAssert sa(js, 1);
			auto path = full_path();
			if (!path.empty()) {
				std::ifstream file(path);
				if (file.is_open()) {
					if (m_entryPoint) {
						duk_module_node_peval_main(js, m_originalPath.c_str());
						js.pop();
					}

					std::stringstream ss;
					ss << file.rdbuf();
					try {
						js.push(ss.str());
						return true;
					}
					catch (const JS::ErrorException& ex) {
						ITEASE_LOGERROR(ex.what() << " in script " << path);
					}
				}
			}
			else js.raise<JS::Error>(JS::Error("cannot find module"));
			js.push(JS::Undefined{ });
			return false;
		}

	private:
		string m_originalPath;
		string m_path;
		string m_parent;
		bool m_entryPoint = false;
		bool m_system = false;
	};
}