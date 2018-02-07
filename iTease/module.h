#pragma once
#include "common.h"
#include "js.h"

namespace iTease {
	class Application;

	class Module {
	public:
		Module(string name) : m_name(name)
		{ }
		virtual ~Module() { }

		inline const string& name() const { return m_name; }
		virtual void init_module(Application&) { }
		virtual void init_js(JS::Context&) { }

	protected:
		const string m_name;
	};
}