#pragma once
#include "module.h"
#include "server.h"
#include "template.h"
#include "js.h"

ITEASE_NAMESPACE_BEGIN
using Path = fs::path;
struct PathJS {
	static constexpr const char* JSName = "\xff""\xff""FSPath";

	static void prototype(JS::Context& js) {
		JS::StackAssert sa(js, 1);

		// return prototype
		js.get_global<void>("\xff""\xff""module-filesystem");
		js.get_property<void>(-1, "Path");
		js.get_property<void>(-1, "prototype");
		js.swap(-1, -3);
		js.pop(2);
	}

	Path path;
};
using PathIterator = pair<shared_ptr<PathJS>, fs::path::iterator>;
struct PathIteratorJS {
	static constexpr const char* JSName = "\xff""\xff""FSPathIterator";
	shared_ptr<PathJS> path;
	fs::path::iterator iterator;
};
class DirectoryIterator {
public:
	static constexpr const char* JSName = "\xff""\xff""FSDirectoryIterator";
};
class RecursiveDirectoryIterator {
public:
	static constexpr const char* JSName = "\xff""\xff""FSRecursiveDirectoryIterator";
};

class FileSystem : public Module {
public:
	FileSystem(Application&);

	virtual void init_js(JS::Context&) override;

private:
	Application& m_app;
};

namespace JS {
	template<>
	class TypeInfo<PathJS> {
	public:
		static PathJS get(JS::Context& js, int idx){
			if (js.is<string>(idx))
				return PathJS{js.get<string>(idx)};
			else if (js.is<Object>(idx)) {
				auto vm = js.get<Object>(idx);
				if (auto ptr = JS::to_object<PathJS>(vm))
					return *ptr;
			}
			return PathJS{};
		}
	};
	template<>
	class TypeInfo<Path> {
	public:
		static Path get(JS::Context& js, int idx) {
			return js.get<PathJS>(idx).path;
		}
		static void push(JS::Context& js, const Path& path) {
			// return new filesystem.path;
			js.get_global<void>("\xff""\xff""module-filesystem");
			js.get_property<void>(-1, "Path");
			js.push(JS::Shared<PathJS>{std::make_shared<PathJS>(PathJS{path})});
			js.create(1);
			js.swap(-1, -2);
			js.pop(1);
		}
	};
	template<>
	class TypeInfo<PathIteratorJS> {
	public:
		static PathIteratorJS get(JS::Context& js, int idx) {
			if (js.is<Object>(idx)) {
				auto vm = js.get<Object>(idx);
				if (auto ptr = JS::to_object<PathIteratorJS>(vm))
					return *ptr;
			}
			return PathIteratorJS{ };
		}
	};
	template<>
	class TypeInfo<PathIterator> {
	public:
		static PathIterator get(JS::Context& js, int idx) {
			auto pathIt = js.get<PathIteratorJS>(idx);
			return std::make_pair(pathIt.path, pathIt.iterator);
		}
	};
}
ITEASE_NAMESPACE_END