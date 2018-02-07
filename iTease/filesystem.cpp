#include "stdinc.h"
#include "common.h"
#include "application.h"
#include "filesystem.h"
#include <climits>
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#elif defined(__unix__)
#include <unistd.h>
#include <fcntl.h>
#endif

ITEASE_NAMESPACE_BEGIN

static fs::path find_base_path() {
	#if defined(_WIN32)
	wchar_t szPathBuffer[MAX_PATH];
	DWORD dwResult = GetModuleFileNameW(NULL, szPathBuffer, std::size(szPathBuffer));
	if (dwResult != 0 && dwResult < std::size(szPathBuffer)) {
		fs::path path(szPathBuffer);
		path.replace_filename(L"");
		return path;
	}
	throw std::runtime_error("find_config_path failed");
	#elif defined(__unix__)
	const char* home_path = std::getenv("HOME");
	std::vector<fs::path> search_path;

	// Search path for unix, ordered by priority:
	{
		// In folder of the binary (temporary installations etc.)
		/*#if defined(__linux__) || defined(__FreeBSD__)
		{
		char path_buffer[PATH_MAX];
		#if defined(__linux__)
		const char* exe_link_path = "/proc/self/exe";
		#elif defined(__FreeBSD__)
		const char* exe_link_path = "/proc/curproc/file";
		#else
		#error
		#endif

		ssize_t outlen = readlink(exe_link_path, path_buffer, std::size(path_buffer) - 1);
		if (outlen != -1) {
		fs::path path(path_buffer, path_buffer + outlen);
		path.replace_filename("");
		search_path.emplace_back(std::move(path));
		}
		}
		#endif*/

		// Home folder
		if (home_path != NULL) {
			search_path.emplace_back(fs::path(home_path) / ".local/share/itease");
		}

		// Generic installation
		search_path.emplace_back("/usr/share/itease");
	}

	for (auto& path : search_path) {
		if (fs::is_directory(path))
			return path;
	}

	throw std::runtime_error("find_config_path failed");
	#else
	#   error find_config_path not implemented for this platform.
	#endif
}

const Path& root_path() {
	static Path path = fs::current_path();
	return path;
}
const Path& config_path() {
	static Path path = root_path() / "config";
	return path;
}
const Path& data_path() {
	static Path path = root_path() / "data";
	return path;
}
const Path& plugin_path() {
	static Path path = root_path() / "plugin";
	return path;
}
const Path& system_path() {
	static Path path = root_path() / "system";
	return path;
}
const Path& web_path() {
	static Path path = root_path() / "web";
	return path;
}

Path get_path_js(JS::Context& js, int idx) {
	return js.get<Path>(idx);
}

int exists_js(JS::Context& js) {
	auto fp = get_path_js(js, 0);
	js.push<bool>(fs::exists(fp));
	return 1;
}
int is_empty_js(JS::Context& js) {
	auto fp = get_path_js(js, 0);
	js.push<bool>(fs::is_empty(fp));
	return 1;
}
int is_file_js(JS::Context& js) {
	auto fp = get_path_js(js, 0);
	js.push<bool>(fs::is_regular_file(fp) || fs::is_symlink(fp));
	return 1;
}
int is_directory_js(JS::Context& js) {
	auto fp = get_path_js(js, 0);
	js.push<bool>(fs::is_directory(fp));
	return 1;
}

FileSystem::FileSystem(Application& app) : Module("filesystem"), m_app(app) {
	m_app.add_interval_callback(10000, [this](long long elapsed) {
		return true;
	});
}

auto FileSystem::init_js(JS::Context& js)->void {
	using namespace std::placeholders;
	JS::StackAssert sa(js, 1);
	js.push_object(
		JS::Property<JS::Object>{"Path", js.push_function_object(
			// constructor
			JS::Function{[this](JS::Context& js) {
				JS::StackAssert sa(js);

				if (!js.is_constructor_call())
					return 0;
				auto path = std::make_shared<Path>(js.get<Path>(0));
				js.construct(JS::Shared<Path>{path});
				js.push(JS::This{});

				js.get_global<void>("\xff""\xff""module-filesystem");
				js.get_property<void>(-1, "Path");
				js.get_property<void>(-1, "prototype");
				js.set_prototype(-4);
				js.pop(3);
				return 0;
			}, 1},
			// filesystem.Path.Iterator
			JS::Property<JS::Object>{"Iterator", js.push_function_object(
				// constructor
				JS::Function{[this](JS::Context& js) {
					JS::StackAssert sa(js);
					if (!js.is_constructor_call())
						return 0;
					auto ptr = js.get<JS::Shared<PathJS>>(0);
					auto it = std::make_shared<PathIteratorJS>(PathIteratorJS{ptr, ptr->path.begin()});
					js.construct(JS::Shared<PathIteratorJS>{it});
					js.push(JS::This{ });

					js.get_global<void>("\xff""\xff""module-filesystem");
					js.get_property<void>(-1, "Path");
					js.get_property<void>(-1, "Iterator");
					js.get_property<void>(-1, "prototype");
					js.set_prototype(-5);
					js.pop(4);
					return 0;
				}, 1},
				// filesystem.Path.Iterator.prototype
				JS::Property<JS::Object>{"prototype", js.push_object(
					JS::Property<bool>{"\xff""\xff""FSPathIterator", true},
					JS::Property<JS::Function>{"next", JS::Function{[this](JS::Context& js) {
						auto& pathIt = *js.self<JS::Shared<PathIteratorJS>>();
						if (pathIt.iterator == pathIt.path->path.end()) {
							js.push_object(
								JS::Property<bool>{"done", true}
							);
						}
						else {
							js.push_object(
								JS::Property<bool>{"done", false},
								JS::Property<Path>{"value", *pathIt.iterator}
							);
							++pathIt.iterator;
						}
						return 1;
					}, 0}}
				)}
			)},
			// filesystem.Path.prototype
			JS::Property<JS::Object>{"prototype", js.push_object(
				JS::Property<bool>{"\xff""\xff""FSPath", true},
				// concatenation
				JS::Property<JS::Function>{"add", JS::Function{[this](JS::Context& js) {
					js.self<JS::Shared<PathJS>>()->path /= js.get<Path>(0);
					js.push(JS::This{});
					return 1;
				}, 1}},
				JS::Property<JS::Function>{"append", JS::Function{[this](JS::Context& js) {
					js.self<JS::Shared<PathJS>>()->path += js.get<Path>(0);
					js.push(JS::This{});
					return 1;
				}, 1}},
				// modifiers
				JS::Property<JS::Function>{"clear", JS::Function{[this](JS::Context& js) {
					js.self<JS::Shared<PathJS>>()->path.clear();
					return 0;
				}, 0}},
				JS::Property<JS::Function>{"clean", JS::Function{[this](JS::Context& js) {
					js.self<JS::Shared<PathJS>>()->path.make_preferred();
					js.push(JS::This{});
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"removeFilename", JS::Function{[this](JS::Context& js) {
					js.self<JS::Shared<PathJS>>()->path.remove_filename();
					js.push(JS::This{});
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"replaceFilename", JS::Function{[this](JS::Context& js) {
					js.self<JS::Shared<PathJS>>()->path.replace_filename(js.get<Path>(0));
					js.push(JS::This{});
					return 1;
				}, 1}},
				JS::Property<JS::Function>{"replaceExtension", JS::Function{[this](JS::Context& js) {
					js.self<JS::Shared<PathJS>>()->path.replace_extension(js.get<Path>(0));
					js.push(JS::This{});
					return 1;
				}, 1}},
				// format observers
				JS::Property<JS::Function>{"toString", JS::Function{[this](JS::Context& js) {
					js.push(to_string(js.self<JS::Shared<PathJS>>()->path.native()));
					return 1;
				}, 0}},
				// decomposition
				JS::Property<JS::Function>{"rootName", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.root_name());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"rootDirectory", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.root_directory());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"rootPath", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.root_path());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"relativePath", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.relative_path());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"parentPath", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.parent_path());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"filename", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.filename());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"stem", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.stem());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"extension", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.extension());
					return 1;
				}, 0}},
				// queries
				JS::Property<JS::Function>{"empty", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.empty());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasRootPath", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_root_path());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasRootName", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_root_name());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasRootDirectory", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_root_directory());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasRelativePath", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_relative_path());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasParentPath", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_parent_path());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasFilename", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_filename());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasStem", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_stem());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"hasExtension", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.has_extension());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"isAbsolute", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.is_absolute());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"isRelative", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<PathJS>>()->path.is_relative());
					return 1;
				}, 0}},
				// iterators
				JS::Property<JS::Function>{"makeIterator", JS::Function{[this](JS::Context& js) {
					js.get_global<void>("\xff""\xff""module-filesystem");
					js.get_property<void>(-1, "Path");
					js.get_property<void>(-1, "Iterator");
					auto path = js.self<JS::Shared<PathJS>>();
					js.push(JS::Shared<PathJS>{path});
					js.create(1);
					js.swap(-1, -3);
					js.pop(2);
					return 1;
				}, 0}}
			)}
		)},
		JS::Property<JS::Object>{"DirectoryIterator", js.push_function_object(
			// constructor
			JS::Function{[this](JS::Context& js) {
				JS::StackAssert sa(js);

				if (!js.is_constructor_call())
					return 0;

				js.get_global<void>("\xff""\xff""module-web");
				js.get_property<void>(-1, "TemplateFile");

				if (!js.instanceof(0, -1))
					js.raise(JS::TypeError(name() + ".TemplateFile required"));
				else {
					js.pop(2);

					auto tfile = js.get<JS::Shared<WebTemplateFile>>(0);
					try {
						auto controller = std::make_shared<WebController>(std::move(tfile));
						js.construct(JS::Shared<WebController>{controller});
						js.push(JS::This{ });
						js.swap(0, -1);
						js.put_property(-2, "template");
						//js.pop();
						js.get_global<void>("\xff""\xff""module-web");
						js.get_property<void>(-1, "Controller");
						js.get_property<void>(-1, "prototype");
						js.set_prototype(-4);
						js.pop(2);
					}
					catch (const std::exception& ex) {
						js.raise(SystemError(errno, ex.what()));
					}
				}
				return 0;
			}, 1},
			// filesystem.DirectoryIterator.prototype
			JS::Property<JS::Object>{"prototype", js.push_object(
				JS::Property<bool>{DirectoryIterator::JSName, true},
				JS::Property<JS::Function>{"onRenderBlock", JS::Function{[this](JS::Context& js) {
					return 0;
				}, 1}}
			)}
		)},
		// filesystem.rootPath
		JS::Property<string>{"rootPath", root_path().string()},
		// filesystem.configPath
		JS::Property<string>{"configPath", config_path().string()},
		// filesystem.dataPath
		JS::Property<string>{"dataPath", data_path().string()},
		// filesystem.pluginPath
		JS::Property<string>{"pluginPath", plugin_path().string()},
		// filesystem.systemPath
		JS::Property<string>{"systemPath", system_path().string()},
		// filesystem.webPath
		JS::Property<string>{"webPath", web_path().string()},
		// filesystem.exists
		JS::Property<JS::Function>{"exists", JS::Function{&exists_js, 1}},
		// filesystem.isEmpty
		JS::Property<JS::Function>{"isEmpty", JS::Function{&is_empty_js, 1}},
		// filesystem.isFile
		JS::Property<JS::Function>{"isFile", JS::Function{&is_file_js, 1}},
		// filesystem.isDirectory
		JS::Property<JS::Function>{"isDirectory", JS::Function{&is_directory_js, 1}}
	);
	js.dup();
	js.put_global("\xff""\xff""module-filesystem");
}

ITEASE_NAMESPACE_END