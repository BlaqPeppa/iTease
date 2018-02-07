#include "stdinc.h"
#include "plugin.h"
#include "script.h"
#include "application.h"

namespace iTease {
	auto from_json(const json& js, PluginData& pd)->void {
		pd.name = js.at("name").get<string>();
		pd.version = js.at("version").get<string>();
		pd.description = js.count("description") ? js.at("description").get<string>() : ""s;
		pd.license = js.count("license") ? js.at("license").get<string>() : ""s;
	}

	auto Plugin::init(Application& app)->void {
		js.put_global("\xff""\xff""plugin", JS::RawPointer<Plugin>{this});
		js.put_global("\xff""\xff""name", m_name);
		js.put_global("\xff""\xff""path", m_path);
		js.put_global("\xff""\xff""parent", m_parent);
		std::ifstream file(m_path);
		if (file.is_open()) {
			std::stringstream ss;
			ss << file.rdbuf();
			try {
				js.peval<void>(ss.str());
			}
			catch (const JS::ErrorException& ex) {
				ITEASE_LOGERROR("Plugin '" << m_name << "': " << ex.what());
				throw ex;
			}
			catch (...) {
				throw;
			}
		}
		else assert(std::false_type::value);
	}
}