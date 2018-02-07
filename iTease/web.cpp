#include "stdinc.h"
#include "web.h"
#include "application.h"
#include "user.h"

using namespace iTease;

namespace iTease {
	void js_add_to_block(Templating::Block* block, const JS::Variant& var) {
		// process objects
		if (var.type() == typeid(JS::VariantMap)) {
			auto vm = std::any_cast<JS::VariantMap>(var);

			// if it's a WebTemplateFile, copy the template node into this templates block
			if (auto tf = JS::to_object<WebTemplateFile>(vm)) {
				*static_cast<Templating::Block*>(block) = *tf->get_template();
			}
			else if (auto tblock = JS::to_object<Templating::Block>(vm)) {
				*static_cast<Templating::Block*>(block) = *tblock;
			}
			else {
				for (auto& pr : vm) {
					if (pr.first == "vars") {
						if (pr.second.type() == typeid(JS::VariantMap)) {
							auto& varmap = std::any_cast<JS::VariantMap>(pr.second);
							for (auto& val : varmap) {
								block->set_var(val.first, convert_any_to_string(val.second));
							}
						}
						else if (pr.second.type() == typeid(JS::VariantVector)) {
							auto& vec = std::any_cast<JS::VariantVector>(pr.second);
							for (auto& val : vec) {
								if (val.type() == typeid(JS::VariantMap)) {
									map<string, string> strmap;
									auto& varmap = std::any_cast<JS::VariantMap>(val);
									for (auto& pr2 : varmap) {
										strmap.emplace(pr2.first, convert_any_to_string(pr2.second));
									}
									block->add_to_array(strmap);
								}
							}
						}
					}
					else if (pr.first == "blocks")
						js_add_to_block(block, pr.second);
					else if (auto blockptr = block->block(pr.first))
						js_add_to_block(blockptr.get(), pr.second);
				}
			}
		}
		// process arrays
		else if (var.type() == typeid(JS::VariantVector)) {
			auto vec = std::any_cast<JS::VariantVector>(var);

			for (auto& val : vec) {
				if (val.type() == typeid(JS::VariantMap)) {
					map<string, string> strmap;
					auto& varmap = std::any_cast<JS::VariantMap>(val);
					for (auto& pr2 : varmap) {
						strmap.emplace(pr2.first, convert_any_to_string(pr2.second));
					}
					block->add_to_array(strmap);
				}
			}
		}
		// process strings
		else if (var.type() == typeid(string)) {
			auto str = std::any_cast<string>(var);
			std::istringstream ss(str);
			block->load(ss);
		}
	}

	const char* get_content_type_by_extension(string_view sv) {
		if (sv == "json")
			return "application/json";
		else if (sv == "css")
			return "text/css";
		else if (sv == "js")
			return "text/javascript";
		else if (sv == "htm" || sv == "html")
			return "text/html";
		else if (sv == "txt" || sv == "text")
			return "text/plain";
		else if (sv == "png")
			return "image/png";
		else if (sv == "jpg" || sv == "jpeg")
			return "image/jpeg";
		else if (sv == "ico")
			return "image/x-icon";
		else if (sv == "gif")
			return "image/gif";
		else if (sv == "bmp")
			return "image/bmp";
		return "";
	}
}

auto WebRequest::prototype(JS::Context& js)->void {
	JS::StackAssert sa(js, 1);

	// set up object
	js.pop();
	js.push_object(
		JS::Property<string>{"uri", m_request.uri.c_str()},
		JS::Property<string>{"method", m_request.method.c_str()}
	);
	// .session
	if (m_request.session) {
		if (m_request.session->data) {
			js.push_object(
				JS::Property<string>{"id", m_request.session->id},
				JS::Property<JS::Shared<User>>{"user", JS::Shared<User>{std::static_pointer_cast<User>(m_request.session->data)}}
			);
		}
		else {
			js.push_object(
				JS::Property<string>{"id", m_request.session->id},
				JS::Property<JS::Undefined>{"user", JS::Undefined{}}
			);
		}
	}
	else {
		js.push_object(
			JS::Property<JS::Undefined>{"id", JS::Undefined{}},
			JS::Property<JS::Undefined>{"user", JS::Undefined{}}
		);
	}

	js.put_property(-2, "session");

	// .params
	js.push(JS::Object{ });
	for (auto& param : m_request.params) {
		js.put_property(-1, param.first, param.second);
	}
	js.put_property(-2, "params");
	// .cookies
	js.push(JS::Object{ });
	for (auto& param : m_request.cookies) {
		js.put_property(-1, param.first, param.second);
	}
	js.put_property(-2, "cookies");
	// .headers
	js.push(JS::Object{ });
	for (auto& param : m_request.headers) {
		js.put_property(-1, param.first, param.second);
	}
	js.put_property(-2, "headers");
	// .post
	js.push(JS::Object{ });
	for (auto& param : m_request.post_data) {
		js.put_property(-1, param.first, param.second);
	}
	js.put_property(-2, "post");

	// return prototype
	js.get_global<void>("\xff""\xff""module-web");
	js.get_property<void>(-1, JSName);
	js.remove(-2);
}

Web::Web(Application& app) : Module("web"), m_app(app)
{ }

void Web::init_js(JS::Context& ctx) {
	using namespace std::placeholders;
	JS::StackAssert sa(ctx, 1);
	ctx.push_object(
		// web.__WebTemplateBlock (prototype)
		JS::Property<JS::Object>{Templating::Block::JSName, ctx.push_object(
			JS::Property<bool>{Templating::Block::JSName, true},
			JS::Property<JS::Function>{"onTemplateRender", JS::Function{[this](JS::Context& js) {
				return 0;
			}, 1}},
			JS::Property<JS::Function>{"onBlockRender", JS::Function{[this](JS::Context& js) {
				return 0;
			}, 2}},
			JS::Property<JS::Function>{"enable", JS::Function{[this](JS::Context& js) {
				auto block = js.self<JS::Shared<Templating::Block>>();
				block->enable_segment(js.require<string>(0));
				return 0;
			}, 1}},
			JS::Property<JS::Function>{"disable", JS::Function{[this](JS::Context& js) {
				auto block = js.self<JS::Shared<Templating::Block>>();
				block->disable_segment(js.require<string>(0));
				return 0;
			}, 1}},
			JS::Property<JS::Function>{"addBlock", JS::Function{[this](JS::Context& js) {
				auto block = js.self<JS::Shared<Templating::Block>>();
				auto name = js.require<string>(0);
				auto target = js.get<string>(1);
				auto new_block = target.empty() ? block->add_block(name) : block->insert_block_at(name, target);
				js.push_this_object(JS::Property<JS::Shared<Templating::Block>>{name, JS::Function{[new_block](JS::Context& js)->duk_ret_t {
					js.push(JS::Shared<Templating::Block>{new_block});
					return 1;
				}, 0}, JS::Function{[new_block](JS::Context& js)->duk_ret_t {
					auto obj = js.get<JS::Object>(0);
					js_add_to_block(new_block.get(), obj);
					return 0;
				}, 1}});

				// blocks
				if (js.has_property(-1, "blocks")) {
					auto blockNames = js.get_property<JS::Array>(-1, "blocks");
					for (auto it = blockNames.begin(); it != blockNames.end(); ++it) {
						if (it->type() != typeid(string)) continue;
						if (std::any_cast<string>(*it) == target) {
							blockNames.insert(++it, name);
							break;
						}
					}
					js.put_property(-1, "blocks", blockNames);
				}

				js.pop();
				js.push(JS::Shared<Templating::Block>{std::move(new_block)});
				return 1;
			}, 2}},
			JS::Property<JS::Function>{"insertBlock", JS::Function{[this](JS::Context& js) {
				auto block = js.self<JS::Shared<Templating::Block>>();
				auto name = js.require<string>(0);
				auto target = js.get<string>(1);
				auto new_block = target.empty() ? block->insert_block(name) : block->add_block_at(name, target);
				js.push_this_object(JS::Property<JS::Shared<Templating::Block>>{name, JS::Function{[new_block](JS::Context& js)->duk_ret_t {
					js.push(JS::Shared<Templating::Block>{new_block});
					return 1;
				}, 0}, JS::Function{[new_block](JS::Context& js)->duk_ret_t {
					auto obj = js.get<JS::Object>(0);
					js_add_to_block(new_block.get(), obj);
					return 0;
				}, 1}});

				// blocks
				if (js.has_property(-1, "blocks")) {
					auto blockNames = js.get_property<JS::Array>(-1, "blocks");
					for (auto it = blockNames.begin(); it != blockNames.end(); ++it) {
						if (it->type() != typeid(string)) continue;
						if (std::any_cast<string>(*it) == target) {
							blockNames.insert(it, name);
							break;
						}
					}
					js.put_property(-1, "blocks", blockNames);
				}
				js.pop();
				js.push(JS::Shared<Templating::Block>{std::move(new_block)});
				return 1;
			}, 2}},
			JS::Property<JS::Function>{"render", JS::Function{[this](JS::Context& js) {
				auto block = js.self<JS::Shared<Templating::Block>>();
				std::stringstream ss;
				block->render(ss);
				js.push(ss.str());
				return 1;
			}, 0}},
			JS::Property<JS::Function>{"onRender", JS::Function{[this](JS::Context& js) {
				return 0;
			}, 2}},
			JS::Property<JS::Function>{"onLoad", JS::Function{[this](JS::Context& js) {
				return 0;
			}, 2}}
		)},
		// web.__WebRequest (prototype)
		JS::Property<JS::Object>{WebRequest::JSName, ctx.push_object(
			JS::Property<bool>{WebRequest::JSName, true}
		)},
		// web.__WebResponse (prototype)
		JS::Property<JS::Object>{WebResponse::JSName, ctx.push_object(
			JS::Property<bool>{WebResponse::JSName, true}
		)},
		// web.Controller
		JS::Property<JS::Object>{"Controller", ctx.push_function_object(
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
			// web.Controller.prototype
			JS::Property<JS::Object>{"prototype", ctx.push_object(
				JS::Property<bool>{WebController::JSName, true},
				JS::Property<JS::Function>{"onRenderBlock", JS::Function{[this](JS::Context& js) {
					return 0;
				}, 1}},
				JS::Property<JS::Function>{"onRequest", JS::Function{[this](JS::Context& js) {
					return 0;
				}, 1}}
			)}
		)},
		// web.LiveResponse
		JS::Property<JS::Object>{"LiveUpdate", ctx.push_function_object(
			// constructor
			JS::Function{[this](JS::Context& js) {
				JS::StackAssert sa(js);
				return 0;
			}, 0},
			// web.LiveResponse.prototype
			JS::Property<JS::Object>{"prototype", ctx.push_object(
				JS::Property<bool>{"\xff""\xff""LiveUpdate", true}/*,
				JS::Property<JS::Function>{"",
				1}*/
			)}
		)},
		// web.TemplateBlock
		JS::Property<JS::Object>{"TemplateBlock", ctx.push_function_object(
			// constructor
			JS::Function{[this](JS::Context& js) {
				if (!js.is_constructor_call())
					return 0;

				try {
					shared_ptr<Templating::Block> block;
					if (js.is<JS::RawPointer<Templating::Block>>(0)) {
						block = std::make_shared<Templating::Block>(*js.get<JS::RawPointer<Templating::Block>>(0));
					}
					else {
						auto name = js.require<string>(0);
						block = std::make_shared<Templating::Block>(name);
					}

					js.construct(JS::Shared<Templating::Block>{block});
					js.push(JS::This{ });
					js.get_global<void>("\xff""\xff""module-web");
					js.get_property<void>(-1, Templating::Block::JSName);
					js.set_prototype(-3);
					js.pop(4);
				}
				catch (const std::exception& ex) {
					js.raise(SystemError(errno, ex.what()));
				}
				return 0;
			}, 1}
		)},
		// web.TemplateFile
		JS::Property<JS::Object>{"TemplateFile", ctx.push_function_object(
			// constructor
			JS::Function{[this](JS::Context& js) {
				JS::StackAssert sa(js);

				if (!js.is_constructor_call())
					return 0;

				auto path = js.require<string>(0);
				try {
					shared_ptr<WebTemplateFile> tfile = js.is<JS::Object>(1) ? std::make_shared<WebTemplateFile>(std::move(path), std::move(js.get<JS::Object>(1)))
						: std::make_shared<WebTemplateFile>(std::move(path));
					js.construct(JS::Shared<WebTemplateFile>{tfile});
					js.construct(*tfile->get_template());
					js.push(JS::This{ });
					js.get_global<void>("\xff""\xff""module-web");
					js.get_property<void>(-1, "TemplateFile");
					js.get_property<void>(-1, "prototype");
					js.set_prototype(-4);
					js.pop(2);

					/*// vars
					js.push_object();
					for (auto var : tfile->get_template().vars()) {
						auto name = var->name();
						js.put_property(-1, JS::Property<string>{name, JS::Function{[name, tfile](JS::Context& js)->duk_ret_t {
							js.push(tfile->get_var(name));
							return 1;
						}, 0}, JS::Function{[tfile, name](JS::Context& js)->duk_ret_t {
							tfile->set_var(name, js.get<string>(-1));
							return 0;
						}, 1}});
					}
					js.put_property(-2, "vars");

					// blocks
					vector<string> blockNames;
					for (auto block : tfile->get_template().blocks()) {
						auto name = block->name();
						blockNames.push_back(name);
						js.put_property(-1, JS::Property<string>{name, JS::Function{[block](JS::Context& js)->duk_ret_t {
							js.push(JS::Shared<Templating::Block>{block});
							if (block->name() == "body") {
								js.print_dump();
							}
							return 1;
						}, 0}, JS::Function{[block](JS::Context& js)->duk_ret_t {
							auto obj = js.get<JS::Object>(0);
							js_add_to_block(block.get(), obj);
							return 0;
						}, 1}});
					}
					// blocks
					js.push(blockNames);
					js.put_property(-2, "blocks");*/

					js.pop();
				}
				catch (const std::exception& ex) {
					js.raise(SystemError(errno, ex.what()));
				}
				return 0;
			}, 2},
			// web.TemplateFile.prototype
			JS::Property<JS::Object>{"prototype", ctx.push_object(
				JS::Property<bool>{WebTemplateFile::JSName, true},
				JS::Property<JS::Function>{"onRender", JS::Function{[this](JS::Context& js) {
					return 0;
				}, 1}},
				JS::Property<JS::Function>{"onRenderBlock", JS::Function{[this](JS::Context& js) {
					return 0;
				}, 1}},
				JS::Property<JS::Function>{"enable", JS::Function{[this](JS::Context& js) {
					auto tfile = js.self<JS::Shared<WebTemplateFile>>();
					tfile->get_template()->enable_segment(js.require<string>(0));
					return 0;
				}, 1}},
				JS::Property<JS::Function>{"disable", JS::Function{[this](JS::Context& js) {
					auto tfile = js.self<JS::Shared<WebTemplateFile>>();
					tfile->get_template()->disable_segment(js.require<string>(0));
					return 0;
				}, 1}},
				JS::Property<JS::Function>{"render", JS::Function{[this](JS::Context& js) {
					auto tfile = js.self<JS::Shared<WebTemplateFile>>();
					std::stringstream ss;
					tfile->get_template()->render(ss);
					js.push(ss.str());
					return 1;
				}, 0}},
				JS::Property<JS::Function>{"addBlock", JS::Function{[this](JS::Context& js) {
					auto tfile = js.self<JS::Shared<WebTemplateFile>>();
					auto block = tfile->get_template()->add_block_at(js.get<string>(0), js.get<string>(1));
					js.push(JS::Shared<Templating::Block>{block});
					return 1;
				}, 2}},
				JS::Property<JS::Function>{"insertBlock", JS::Function{[this](JS::Context& js) {
					auto tfile = js.self<JS::Shared<WebTemplateFile>>();
					auto block = tfile->get_template()->insert_block_at(js.get<string>(0), js.get<string>(1));
					js.push(JS::Shared<Templating::Block>{block});
					return 1;
				}, 2}},
				JS::Property<JS::Function>{"setVar", JS::Function{[this](JS::Context& js) {
					auto name = js.require<string>(0);
					js.self<JS::Shared<WebTemplateFile>>()->set_var(name, js.require<string>(1));
					js.push(JS::This{ });
					if (!js.has_property(-1, name)) {
						js.put_property(-1, JS::Property<string>{name, JS::Function{[name](JS::Context& js)->duk_ret_t {
							js.push(js.self<JS::Shared<WebTemplateFile>>()->get_var(name));
							return 1;
						}, 0}, JS::Function{[name](JS::Context& js)->duk_ret_t {
							js.self<JS::Shared<WebTemplateFile>>()->set_var(name, js.get<string>(-1));
							return 0;
						}, 1}});
					}
					js.pop();
					return 0;
				}, 2}},
				JS::Property<JS::Function>{"getVar", JS::Function{[this](JS::Context& js) {
					js.push(js.self<JS::Shared<WebTemplateFile>>()->get_var(js.require<string>(0)));
					return 1;
				}, 1}},
				JS::Property<JS::Function>{"cacheFile", JS::Function{[this](JS::Context& js) {
					if (js.is<string>(1))
						js.push(js.self<JS::Shared<WebTemplateFile>>()->cache_file(js.require<string>(0), js.get<string>(1)));
					else js.push(js.self<JS::Shared<WebTemplateFile>>()->cache_file(js.get<string>(0)));
					return 1;
				}, 2}}
			)}
		)},
		// web.addController
		JS::Property<JS::Function>{"addController", JS::Function{std::bind(&Web::add_controller_js, this, _1), 1}},
		// web.listen
		JS::Property<JS::Function>{"listen", JS::Function{std::bind(&Web::add_listener_js, this, _1), 2}},
		// web.isStaticFile
		JS::Property<JS::Function>{"isStaticFile", JS::Function{std::bind(&Web::is_static_file_js, this, _1), 1}},
		// web.createStaticResponse
		JS::Property<JS::Function>{"createStaticResponse", JS::Function{std::bind(&Web::create_static_response_js, this, _1), 1}}
	);
	ctx.dup();
	ctx.put_global("\xff""\xff""module-web");
}
WebResponse Web::create_static_response(const fs::path& path) {
	ServerResponse response;
	auto fpath = fs::path("web") / path;
	if (fs::is_regular_file(fpath) || fs::is_symlink(fpath)) {
		std::ifstream file(fpath, std::ios::binary);
		if (file.is_open()) {
			auto fsize = fs::file_size(fpath);
			file.unsetf(std::ios::skipws);
			response.content.reserve(static_cast<size_t>(fsize));
			response.content.insert(response.content.begin(), std::istream_iterator<char>(file), std::istream_iterator<char>());
			response.content_type = get_content_type_by_extension(string(ltrim(fpath.extension().string(), ".")));
			response.status = 200;
		}
	}
	return response;
}
void Web::add_controller(shared_ptr<WebController> ptr) {
	m_controllers.emplace_back(ptr);
}
void Web::add_listener(Server::OnRequestEvent::Handler func) {
	m_serverRequestListeners.push_back(m_app.OnServerRequest.listen(func));
}
int Web::create_static_response_js(JS::Context& js) {
	JS::StackAssert sa(js, 1);
	auto t1 = js.top();
	js.push(create_static_response(js.require<string>(0)));
	auto t2 = js.top();
	if (t2 == t1) {
		js.print_dump();
		t1 = t2;
	}
	return 1;
}
int Web::add_listener_js(JS::Context& js) {
	if (!js.is<JS::Function>(0))
		js.raise(JS::TypeError{"function"});

	auto listenerCB = std::make_shared<JS::Callback<JS::Shared<WebRequest>>>(js, 0);
	add_listener([this, listenerCB](const ServerRequest& req, ServerResponse& resp) {
		auto& js = listenerCB->context();
		JS::StackAssert sa(js);
		(*listenerCB)(JS::Shared<WebRequest>{std::move(std::make_shared<WebRequest>(req))});
		if (js.is<WebResponse>(-1)) {
			resp = js.get<WebResponse>(-1).response;
			js.pop();
			return true;
		}
		else {
			js.get_global<void>("\xff""\xff""module-web");
			js.get_property<void>(-1, "Controller");
			if (js.instanceof(-3, -1)) {
				js.pop(2);

				auto controller = js.get<JS::Shared<WebController>>(-1);
				auto result = controller->request(js, req, resp);
				js.pop();
				return result;
			}
			else js.pop(3);
		}
		return false;
	});
	return 0;
}
int Web::is_static_file_js(JS::Context& js) {
	JS::StackAssert sa(js, 1);
	auto path = fs::path("web") / js.require<string>(0);
	js.push<bool>(fs::is_regular_file(path));
	return 1;
}
int Web::add_controller_js(JS::Context& js) {
	JS::StackAssert sa(js);

	js.get_global<void>("\xff""\xff""module-web");
	js.get_property<void>(-1, "Controller");
	if (!js.instanceof(0, -1))
		js.raise(JS::TypeError(name() + ".Controller"));;
	js.pop(2);

	// add callback for template.onRender
	js.get_property<void>(0, "template");
	js.get_property<void>(-1, "onRender");

	bool onRenderCB = js.is<JS::Function>(-1);
	shared_ptr<JS::CallbackMethod<>> jsOnRenderCB;
	if (onRenderCB) {
		js.swap(-1, -2);

		jsOnRenderCB = std::make_shared<JS::CallbackMethod<>>(js);
		js.pop(2);
	}

	auto controller = js.get<JS::Shared<WebController>>(0);
	if (onRenderCB) {
		controller->get_web_template()->get_template()->OnRenderBlock.connect([jsOnRenderCB](Templating::Node& node) {
			(*jsOnRenderCB)();
			jsOnRenderCB->context().pop();
			return true;
		});
	}

	// add callback for template.onRenderBlock
	js.get_property<void>(0, "template");
	js.get_property<void>(-1, "onRenderBlock");

	onRenderCB = js.is<JS::Function>(-1);
	shared_ptr<JS::CallbackMethod<JS::Pointer<Templating::Block>>> jsOnRenderBlockCB;
	if (onRenderCB) {
		js.swap(-1, -2);

		jsOnRenderBlockCB = std::make_shared<JS::CallbackMethod<JS::Pointer<Templating::Block>>>(js);
		js.pop(2);
	}

	controller = js.get<JS::Shared<WebController>>(0);
	if (onRenderCB) {
		controller->get_web_template()->get_template()->OnRenderBlock.connect([jsOnRenderBlockCB](Templating::Block& block) {
			(*jsOnRenderBlockCB)(JS::Pointer<Templating::Block>{&block});
			jsOnRenderBlockCB->context().pop();
			return true;
		});
	}

	add_controller(controller);
	return 0;
}

WebTemplateFile::WebTemplateFile(string path) : m_template(std::make_shared<Templating::Block>()) {
	std::ifstream file(path);
	if (!file.is_open()) throw(std::runtime_error("failed to open template '" + path + "'"));
	m_template->load(file);
}
WebTemplateFile::WebTemplateFile(string path, const JS::VariantMap& data) : m_template(std::make_shared<Templating::Block>()) {
	std::ifstream file(path);
	if (!file.is_open()) throw(std::runtime_error("failed to open template '" + path + "'"));
	m_template->load(file);
	auto it = data.find("vars");
	if (it != data.end()) {
		if (it->second.type() == typeid(JS::VariantMap))
			add_vars(std::any_cast<JS::VariantMap>(it->second));
		else if (it->second.type() == typeid(JS::VariantVector))
			add_var_array(std::any_cast<JS::VariantVector>(it->second));
	}
	it = data.find("blocks");
	if (it != data.end() && it->second.type() == typeid(JS::VariantMap)) {
		add_blocks(std::any_cast<JS::VariantMap>(it->second));
	}
}
WebTemplateFile::~WebTemplateFile() {
	if (m_cacheFile.size()) fs::remove(m_cacheFile);
}
void WebTemplateFile::set_var(const string& name, const string& value) {
	m_template->set_var(name, value);
}
string WebTemplateFile::get_var(const string& name) const {
	return m_template->has_var(name) ? m_template->var(name).value() : "";
}
shared_ptr<Templating::Block> WebTemplateFile::get_block(const string& name) const {
	if (auto block = m_template->block(name)) {
		return block;
	}
	return std::make_shared<Templating::Block>(name);
}
string WebTemplateFile::cache_file(const string& path) {
	std::ofstream file;

	// if there's already a cache file present, then if (path == "" or path == cacheFile), we can update it the existing file, else remove the old file
	if (!m_cacheFile.empty()) {
		auto fp = fs::path("web/cache") / path;
		if (path.empty() || fp == m_cacheFile) {
			file.open(m_cacheFile, std::ios::trunc);
		}
		else {
			fs::remove(m_cacheFile);
			m_cacheFile = "";
		}
	}
	// if a path is provided, open a new file
	if (!file.is_open() && !path.empty()) {
		auto fp = fs::path("web/cache") / path;
		fs::create_directories(fp.parent_path());
		file.open(fp);
		if (file.is_open())
			m_cacheFile = fp.string();
	}
	// render to the cache file
	if (file.is_open()) {
		m_template->render(file);
		file.close();
		return "/cache/" + str_replaced(fs::path(path).make_preferred().string(), "\\", "/");
	}
	return "";
}
string WebTemplateFile::cache_file(const string& prefix, const string& suffix) {
	auto md5 = rand_md5();
	auto str = prefix + md5 + suffix;
	return cache_file(str);
}
void WebTemplateFile::add_vars(const JS::VariantMap& map) {
	for (auto& pr : map) {
		m_template->set_var(pr.first, convert_any_to_string(pr.second));
	}
}
void WebTemplateFile::add_var_array(const JS::VariantVector& vm) {
	vector<map<string, string>> vec;
	for (auto& var : vm) {
		if (var.type() != typeid(JS::VariantMap)) continue;
		auto varmap = std::any_cast<JS::VariantMap>(var);
		map<string, string> vars;
		for (auto& pr : varmap) {
			vars.emplace(pr.first, convert_any_to_string(pr.second));
		}
		vec.emplace_back(vars);
	}
	m_template->set_array(vec);
}
void WebTemplateFile::add_blocks(const JS::VariantMap& map) {
	for (auto& pr : map) {
		if (auto block = m_template->block(pr.first)) {
			add_to_block(block.get(), pr.second);
		}
	}
}
void WebTemplateFile::add_to_block(Templating::Block* block, const JS::Variant& var) {
	js_add_to_block(block, var);
	block->set_parent(m_template.get());

	// add vars from this template to the sub-block
	auto vars = m_template->vars();
	for (auto& var : vars) {
		if (block->has_var(var->name()))
			block->set_var(var->name(), var->value());
	}
}

WebController::WebController(shared_ptr<WebTemplateFile> templateFile) : m_template(templateFile) { }
auto WebController::request(JS::Context& js, const ServerRequest& req, ServerResponse& resp)->bool {
	JS::StackAssert sa(js);
	std::ostringstream ss;
	js.dup();
	js.get_property<void>(-1, "onRequest");
	auto block = m_template->get_template().get();

	if (js.is<JS::Function>(-1)) {
		js.swap(-1, -2);
		js.push(JS::Shared<WebRequest>{std::make_shared<WebRequest>(req)});
		js.pcall_method(1);
		if (!js.is<JS::Undefined>(-1)) {
			// check for handled types
			if (js.is<JS::Object>(-1)) {
				auto vm = js.get<JS::Object>(-1);
				if (auto tf = JS::to_object<WebTemplateFile>(vm)) {
					block = tf->get_template().get();
				}
				else if (auto tb = JS::to_object<Templating::Block>(vm)) {
					block = tb;
				}
				else if (js.is<WebResponse>(-1)) {
					resp = js.get<WebResponse>(-1).response;
					js.pop();
					return true;
				}
				else {
					// return jsonified JS object
					resp.content = js.encode(-1);
					resp.content_type = get_content_type_by_extension("json");
					resp.status = 200;
					js.pop();
					return true;
				}
			}
			else {
				// return plain text string
				if (js.is<string>(-1)) {
					resp.content = js.get<string>(-1);
					resp.content_type = get_content_type_by_extension("text");
					resp.status = 200;
					js.pop();
					return true;
				}
				// return jsonified JS array/whatever
				resp.content = js.encode(-1);
				resp.content_type = get_content_type_by_extension("json");
				resp.status = 200;
				js.pop();
				return true;
			}
		}
		js.pop();
	}

	js.dup();
	js.get_property<void>(-1, "onRenderBlock");

	{
		shared_ptr<JS::CallbackMethod<Templating::Block&>> jsOnRenderCB;
		Templating::OnBlockRender::Listener onRenderBlock;
		if (js.is<JS::Function>(-1)) {
			js.swap(-1, -2);
			jsOnRenderCB = std::make_shared<JS::CallbackMethod<Templating::Block&>>(js);
			js.pop(2);

			onRenderBlock = m_template->get_template()->OnRenderBlock.listen([&js, jsOnRenderCB](Templating::Block& block) {
				(*jsOnRenderCB)(block);
				js.pop();
				return true;
			});
		}
		block->render(ss);
	}
	resp.content = ss.str();
	resp.status = 200;
	resp.content_type = get_content_type_by_extension("html");
	return true;
}