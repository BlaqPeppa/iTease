#pragma once
#include "common.h"
#include "module.h"
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
#endif

namespace iTease {
	extern const fs::path& root_path();
	extern const fs::path& config_path();
	extern const fs::path& data_path();
	extern const fs::path& plugin_path();
	extern const fs::path& system_path();
	extern const fs::path& web_path();

	class System : public Module {
	public:
		System();
		virtual void init_js(JS::Context&) override;
	};
}