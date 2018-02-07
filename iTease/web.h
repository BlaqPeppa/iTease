#pragma once
#include "module.h"
#include "server.h"
#include "template.h"

namespace iTease {
	extern void js_add_to_block(Templating::Block*, const JS::Variant&);

	class WebTemplateBase {
	public:
		virtual ~WebTemplateBase() { }
	};

	class WebTemplateFile : public WebTemplateBase {
	public:
		static constexpr const char* JSName = "\xff""\xff""WebTemplateFile";

		WebTemplateFile(string path);
		WebTemplateFile(string path, const JS::VariantMap& data);
		virtual ~WebTemplateFile();

		void set_var(const string& name, const string& value);
		string get_var(const string& name) const;
		shared_ptr<Templating::Block> get_block(const string& name) const;
		// cache's the rendered file into the web directory, returns the cache'd web path
		string cache_file(const string& path);
		string cache_file(const string& prefix, const string& suffix);

		auto get_template() {
			return m_template;
		}

	private:
		void add_vars(const JS::VariantMap&);
		void add_var_array(const JS::VariantVector&);
		void add_blocks(const JS::VariantMap&);
		void add_to_block(Templating::Block*, const JS::Variant&);

	private:
		shared_ptr<Templating::Block> m_template;
		string m_cacheFile;
	};

	class WebController {
	public:
		static constexpr const char* JSName = "\xff""\xff""WebController";

		WebController(shared_ptr<WebTemplateFile> templateFile);
		virtual ~WebController() { }

		auto get_web_template()->shared_ptr<WebTemplateFile> { return m_template; }

		auto request(JS::Context&, const ServerRequest& request, ServerResponse& response)->bool;

	protected:
		shared_ptr<WebTemplateFile> m_template;
		Templating::OnTemplateRender::Listener m_onTemplateRender;
		Templating::OnBlockRender::Listener m_onBlockRender;
	};

	class WebResponse {
	public:
		WebResponse() = default;
		WebResponse(const ServerResponse& resp) : response(resp)
		{ }
		virtual ~WebResponse() { }

	public:
		static constexpr const char* JSName = "\xff""\xff""WebResponse";

		void prototype(JS::Context& js) {
			JS::StackAssert sa(js, 1);

			// set up object
			js.put_properties(-1,
				JS::Property<string>{"uri", response.content},
				JS::Property<string>{"method", response.content_type},
				JS::Property<int>{"status", response.status}
			);

			// return prototype
			js.get_global<void>("\xff""\xff""module-web");
			js.get_property<void>(-1, JSName);
			js.remove(-2);
		}

	public:
		ServerResponse response;
	};

	class WebRequest {
	public:
		WebRequest(const ServerRequest& req) : m_request(req)
		{ }

		virtual ~WebRequest() { }

	public:
		static constexpr const char* JSName = "\xff""\xff""WebRequest";

		void prototype(JS::Context& js);

	protected:
		ServerRequest m_request;
	};

	class Web : public Module {
	public:
		Web(Application&);
		virtual void init_js(JS::Context&) override;

		void add_controller(shared_ptr<WebController>);
		void add_listener(Server::OnRequestEvent::Handler);
		WebResponse create_static_response(const fs::path& path);

	private:
		int create_static_response_js(JS::Context&);
		int add_controller_js(JS::Context&);
		int add_listener_js(JS::Context&);
		int is_static_file_js(JS::Context&);
		void add_block_controller_js(JS::Context&, const string& name, const JS::VariantMap& block);

	private:
		Application& m_app;
		vector<shared_ptr<WebController>> m_controllers;
		vector<Server::OnRequestEvent::Listener> m_serverRequestListeners;
	};


	namespace JS {
		template <>
		class TypeInfo<Templating::Node> {
		public:
			static void apply(Context& js, Templating::Node& node) {
				// options
				js.push_object();
				auto blockptr = node.shared_from_this();
				for (auto opt : node.options()) {
					js.put_property(-1, Property<string>{opt.first, opt.second, DUK_DEFPROP_SET_ENUMERABLE | DUK_DEFPROP_SET_CONFIGURABLE});
				}
				js.put_property(-2, "opts");
				// vars
				js.push_object();
				for (auto var : node.vars()) {
					auto name = var->name();
					js.put_property(-1, Property<string>{var->name(), JS::Function{[var](JS::Context& js)->duk_ret_t {
						js.push(var->value());
						return 1;
					}, 0}, JS::Function{[blockptr, var](JS::Context& js)->duk_ret_t {
						blockptr->set_var(var->name(), js.get<string>(0));
						return 0;
					}, 1}});
				}
				js.put_property(-2, "vars");
				// blocks
				vector<string> blockNames;
				for (auto sub_block : node.blocks()) {
					auto name = sub_block->name();
					blockNames.push_back(name);
					js.put_property(-1, Property<JS::Shared<Templating::Block>>{name, JS::Function{[sub_block](JS::Context& js)->duk_ret_t {
						js.push(JS::Shared<Templating::Block>{sub_block});
						return 1;
					}, 0}, JS::Function{[sub_block](JS::Context& js)->duk_ret_t {
						auto obj = js.get<Object>(0);
						js_add_to_block(sub_block.get(), obj);
						return 0;
					}, 1}});
				}
				js.push(blockNames);
				js.put_property(-2, "blocks");
			}
			static void push(Context& js, Templating::Node& node) {
				StackAssert sa(js, 1);
				js.push_object();
				apply(js, std::move(node));
				js.pop();
			}
			static void construct(Context& js, Templating::Node& node) {
				js.push_this_object();
				apply(js, std::move(node));
				js.pop();
			}
		};

		template <>
		class TypeInfo<Templating::Block> {
		public:
			static void apply(Context& js, Templating::Block& block) {
				TypeInfo<Templating::Node>::apply(js, block);
				auto blockptr = std::static_pointer_cast<Templating::Block>(block.shared_from_this());
				js.put_properties(-1,
					Property<string>{"name", block.name()},
					Property<string>{"path", block.path()},
					Property<bool>{"enabled", JS::Function{[blockptr](JS::Context& js)->int {
						//js.push<bool>(js.self<Shared<Templating::Block>>()->enabled());
						js.push<bool>(blockptr->enabled());
						return 1;
					}, 0}, JS::Function{[blockptr](JS::Context& js)->int {
						//js.self<Shared<Templating::Block>>()->enable(js.get<bool>(0));
						blockptr->enable(js.get<bool>(0));
						return 0;
					}, 1}},
					Property<JS::Undefined>{"vars", JS::Function{[blockptr](JS::Context& js)->duk_ret_t {
						if (blockptr->array_size()) {
							js.push(JS::Array{ });
							for (size_t i = 0; i < blockptr->array_size(); ++i) {
								js.push_object();
								auto& arr = blockptr->get_array()[i];
								for (auto& pr : arr) {
									auto name = pr.first;
									js.put_property(-1, Property<string>{pr.first, JS::Function{[blockptr, name, i](JS::Context& js)->duk_ret_t {
										auto& arr = blockptr->get_array();
										if (arr.size() <= i) arr.resize(i + 1);
										js.push(blockptr->get_array()[i][name]);
										return 1;
									}, 0}, JS::Function{[blockptr, name, i](JS::Context& js)->duk_ret_t {
										auto& arr = blockptr->get_array();
										if (arr.size() <= i) arr.resize(i + 1);
										arr[i][name] = js.get<string>(0);
										return 0;
									}, 1}});
								}
								js.put_property(-2, i);
							}
						}
						else {
							js.push_object();
							for (auto var : blockptr->vars()) {
								auto name = var->name();
								js.put_property(-1, Property<string>{var->name(), JS::Function{[var](JS::Context& js)->duk_ret_t {
									js.push(var->value());
									return 1;
								}, 0}, JS::Function{[blockptr, var](JS::Context& js)->duk_ret_t {
									blockptr->set_var(var->name(), js.get<string>(0));
									return 0;
								}, 1}});
							}
						}
						return 1;
					}, 0}, JS::Function{[blockptr](JS::Context& js)->duk_ret_t {
						if (!js.is<Array>(0) && !js.is<Object>(0))
							js.raise(JS::ParameterError("array or object required"));
						if (js.is<Array>(0)) {
							vector<map<string, string>> arr;
							auto vec = js.get<Array>(0);
							for (auto& var : vec) {
								auto& map = arr.emplace_back();
								if (var.type() == typeid(VariantMap)) {
									auto vm = std::any_cast<JS::VariantMap>(var);
									for (auto& pr : vm) {
										map.emplace(pr.first, convert_any_to_string(pr.second));
									}
								}
							}
							blockptr->set_array(arr);
						}
						else {
							auto obj = js.get<Object>(0);
							for (auto& var : obj) {
								blockptr->set_var(var.first, convert_any_to_string(var.second));
							}
						}
						return 0;
					}, 1}}
				);
			}
			static void push(Context& js, Templating::Block& block) {
				StackAssert sa(js, 1);
				js.push_object();
				apply(js, std::move(block));
			}
			static void construct(Context& js, Templating::Block& block) {
				js.push_this_object();
				apply(js, std::move(block));
				js.pop();
			}
		};

		template<>
		class TypeInfo<WebResponse> {
		public:
			static void push(Context& js, const WebResponse& wr) {
				StackAssert sa(js, 1);
				js.push_object(
					Property<string>{"content", wr.response.content},
					Property<string>{"content_type", wr.response.content_type},
					Property<int>{"status", wr.response.status}
				);
			}
			static bool is(Context& js, int idx) {
				return js.is<Object>(idx) && js.has_property(idx, "content") && js.has_property(idx, "content_type") && js.has_property(idx, "status");
			}
			static WebResponse get(Context& js, int idx) {
				auto resp = ServerResponse();
				resp.content = js.get_property<string>(idx, "content");
				resp.content_type = js.get_property<string>(idx, "content_type");
				resp.status = js.get_property<int>(idx, "status");
				return resp;
			}
		};
	}
}