#pragma once
#include "common.h"
#include "module.h"
#include "template.h"
#include "web.h"

namespace iTease {
	class Application;

	class WebUIUpdate {
			
	};

	class WebUI : public Module {
	public:
		WebUI(Application& app) : Module("webui"), m_app(app)
		{ }

	private:
		Application& m_app;
	};
}