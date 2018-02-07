#pragma once
#include "common.h"
#include "js.h"

namespace iTease {
	class Application;

	struct PluginData {
		string name;

		string version{"unknown"};
		string license{"unknown"};
		string description{"unknown"};
	};

	class Plugin : public std::enable_shared_from_this<Plugin> {
	public:
		Plugin(PluginData data, string plugin_path) : m_data(std::move(data)), m_path(std::move(plugin_path)) {
			JS::StackAssert sa(js);
			fs::path path = m_path;
			m_name = path.stem().string();
			m_parent = (path.is_absolute() ? path.parent_path() : fs::current_path()).string();
		}

		auto& path() const { return m_path; }
		auto& data() const { return m_data; }

		auto init(Application&)->void;

	public:
		JS::Context js;

	private:
		string m_name;
		string m_path;
		string m_parent;

		PluginData m_data;
	};

	auto from_json(const json&, PluginData&)->void;
}