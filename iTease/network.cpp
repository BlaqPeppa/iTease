#include "stdinc.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include "application.h"
#include "network.h"

using namespace iTease;

NetRequest::NetRequest(string url) : NetRequest(NetRequestOptions{url})
{ }
NetRequest::NetRequest(NetRequestOptions opts) : NetRequest(curl_easy_init(), opts)
{ }
NetRequest::NetRequest(CURL* curl, NetRequestOptions opts) : m_curl(curl), opts(opts) {
	if (!m_curl) throw std::runtime_error("curl init error");
}
NetRequest::NetRequest(NetRequest&& v) : opts(std::move(v.opts)), m_data(std::move(v.m_data)), m_applied(v.m_applied), m_finished(v.m_finished) {
	std::swap(m_curl, v.m_curl);
}
NetRequest::~NetRequest() NOEXCEPT {
	if (m_curl) {
		curl_easy_cleanup(m_curl);
		m_curl = nullptr;
	}
}
auto NetRequest::operator==(const NetRequest& v) const->bool {
	return m_curl == v.m_curl;
}
auto NetRequest::clone() const->shared_ptr<NetRequest> {
	return std::make_shared<NetRequest>(curl_easy_duphandle(m_curl), opts);
}
auto NetRequest::apply()->void {
	if (m_applied) return;
	ITEASE_LOGDEBUG("cURL Request: " << opts.url);
	curl_easy_setopt(m_curl, CURLOPT_URL, opts.url.c_str());
	curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, opts.progress ? 0L : 1L);
	curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, opts.follow ? 1L : 0L);
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
	#if _DEBUG
	curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 0L);
	#endif
	if (opts.timeout) curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, *opts.timeout);
	m_applied = true;
}
auto NetRequest::perform()->void {
	apply();
	const CURLcode code = curl_easy_perform(m_curl);
	m_finished = true;
	if (code != CURLE_OK)
		throw NetRequestException(code);
	OnRequestComplete(shared_from_this(), NetResponseInfo(shared_from_this(), code));
}
auto NetRequest::pause(const int bitmask)->void {
	auto code = curl_easy_pause(m_curl, bitmask);
	if (code != CURLE_OK) throw NetRequestException(code);
}
auto NetRequest::reset() NOEXCEPT->void {
	curl_easy_reset(m_curl);
}
auto NetRequest::curl() const->CURL* {
	return m_curl;
}
auto NetRequest::write_func(char* d, size_t n, size_t l, void* p)->size_t {
	if (!p) return 0;
	auto& req = *reinterpret_cast<NetRequest*>(p);
	auto size = n * l;
	req.m_data.insert(req.m_data.end(), d, d + size);
	req.OnWriteData(req.shared_from_this());
	return size;
}

auto NetMultiRequest::curl_multi_deleter::operator()(CURLM* ptr) const->void {
	curl_multi_cleanup(ptr);
}

NetMultiRequest::NetMultiRequest() : m_curl(curl_multi_init())
{ }
NetMultiRequest::NetMultiRequest(NetMultiRequest&& v) : m_curl(std::move(v.m_curl)), m_numActive(v.m_numActive)
{ }
NetMultiRequest::~NetMultiRequest() NOEXCEPT = default;

NetMultiRequest& NetMultiRequest::operator=(NetMultiRequest&& v) {
	if (this != &v) {
		m_numActive = v.m_numActive;
		m_curl = std::move(v.m_curl);
	}
	return *this;
}
auto NetMultiRequest::add(NetRequestOptions opts)->shared_ptr<Listener> {
	auto ptr = std::make_shared<NetRequest>(opts);
	const CURLMcode code = curl_multi_add_handle(m_curl.get(), ptr->curl());
	if (code != CURLM_OK) throw NetMultiException(code);
	auto listener = std::make_shared<Listener>(std::move(Listener{*this, ptr.get()}));
	m_handles.emplace(ptr->curl(), std::move(ptr));
	return listener;
}
auto NetMultiRequest::remove(NetRequest* req)->void {
	const CURLMcode code = curl_multi_remove_handle(m_curl.get(), req->curl());
	if (code != CURLM_OK) throw NetMultiException(code);
	m_handles.erase(req->curl());
}
auto NetMultiRequest::perform()->bool {
	auto numActive = m_numActive;
	auto num = m_handles.size();
	if (!num) return true;
	for (auto& h : m_handles) {
		h.second->apply();
	}
	const CURLMcode code = curl_multi_perform(m_curl.get(), &m_numActive);
	if (numActive > m_numActive) {
		CURLMsg* msg;
		while ((msg = curl_multi_info_read(m_curl.get(), &m_numQueued))) {
			if (msg->msg == CURLMSG_DONE) {
				auto& handle = *m_handles[msg->easy_handle];
				handle.OnRequestComplete(handle.shared_from_this(), NetResponseInfo(handle.shared_from_this(), msg));
				remove(&handle);
			}
		}
	}
	if (code == CURLM_CALL_MULTI_PERFORM)
		return false;
	if (code != CURLM_OK)
		throw NetMultiException(code);
	return true;
}
auto NetMultiRequest::get_info(NetRequest* req)->optional<NetResponseInfo> {
	CURLMsg* msg = nullptr;
	while ((msg = curl_multi_info_read(m_curl.get(), &m_numQueued))) {
		if (msg->easy_handle == req)
			return std::make_optional<NetResponseInfo>(m_handles[msg->easy_handle]->shared_from_this(), msg);
	}
	return std::nullopt;
}
auto NetMultiRequest::get_info()->vector<NetResponseInfo> {
	vector<NetResponseInfo> infos;
	CURLMsg* msg = nullptr;
	while ((msg = curl_multi_info_read(m_curl.get(), &m_numQueued))) {
		infos.emplace_back(m_handles[msg->easy_handle]->shared_from_this(), msg);
	}
	return infos;
}
auto NetMultiRequest::is_finished(shared_ptr<NetRequest> req)->bool {
	CURLMsg* msg = nullptr;
	while ((msg = curl_multi_info_read(m_curl.get(), &m_numQueued))) {
		if (msg->easy_handle == req.get() && msg->msg == CURLMSG_DONE)
			return true;
	}
	return false;
}
NetResponseInfo::NetResponseInfo(NetRequestPtr ptr, const CURLMsg* msg) : message(msg->msg), data(msg->data.whatever), code(msg->data.result), request(ptr)
{ }
NetResponseInfo::NetResponseInfo(NetRequestPtr ptr, CURLcode code) : request(ptr), code(code)
{ }

Network::Network(Application& app) : Module("net"), m_app(app)
{ }
Network::~Network() { }
auto Network::run()->void {
	m_requests->perform();
	//curl_multi_perform(m_curl, );
	//curl_multi_wait(m_curl, nullptr, 0, 1000, )
}
auto Network::init_module(Application& app)->void {
	m_requests = std::make_shared<NetMultiRequest>();
	m_onTick = m_app.add_interval_callback(100, [this](long long& interval) {
		m_requests->perform();
		return true;
	});
}
auto Network::init_js(JS::Context& ctx)->void {
	using namespace std::placeholders;
	JS::StackAssert sa(ctx, 1);
	ctx.push_object(
		// net.__NetResponseInfo (prototype)
		JS::Property<JS::Object>{NetResponseInfo::JSName, ctx.push_object(
			JS::Property<bool>{NetResponseInfo::JSName, true}
		)},
		// net.Request
		JS::Property<JS::Object>{"Request", ctx.push_function_object(
			// constructor
			JS::Function{[this](JS::Context& js) {
				JS::StackAssert sa(js);

				if (!js.is_constructor_call())
					return 0;

				NetRequestOptions opts;
				// first arg [string/object]
				if (js.is<string>(0)) {
					opts.url = js.get<string>(0);
					if (js.is<JS::Object>(1)) opts = js.get<JS::Object>(1);
				}
				else if (js.is<JS::Object>(0)) opts = js.get<JS::Object>(0);
				else js.raise(JS::TypeError("function or object"));

				auto listener = m_requests->add(opts);
				js.construct(JS::Shared<NetRequestJS>{std::make_shared<NetRequestJS>(listener)});
				js.push(JS::This{ });
				//js.pop();
				js.get_global<void>("\xff""\xff""module-net");
				js.get_property<void>(-1, "Request");
				js.get_property<void>(-1, "prototype");
				js.set_prototype(-4);
				js.pop(2);
				return 0;
			}, 1},
			// net.Request.prototype
			JS::Property<JS::Object>{"prototype", ctx.push_object(
				JS::Property<bool>{NetRequestJS::JSName, true},
				JS::Property<JS::Function>{"fail", JS::Function{[this](JS::Context& js) {
					if (!js.is<JS::Function>(0))
						js.raise(JS::TypeError("function"));
					JS::StackAssert sa(js, 1);
					auto req = js.self<JS::Shared<NetRequestJS>>();
					js.push(JS::This{});
					auto cb = std::make_shared<JS::CallbackMethod<string, NetResponseInfo>>(js);
					req->doneListeners.emplace_back(req->listener->request().OnRequestComplete.listen([cb](NetRequestPtr listener, const NetResponseInfo& info) {
						JS::StackAssert sa(cb->context());
						auto& js = cb->context();
						auto str = listener->data_string();
						(*cb)(str, info);
						js.pop();
						return true;
					}));
					return 1;
				}, 1}},
				JS::Property<JS::Function>{"done", JS::Function{[this](JS::Context& js) {
					if (!js.is<JS::Function>(0))
						js.raise(JS::TypeError("function"));
					JS::StackAssert sa(js, 1);
					auto req = js.self<JS::Shared<NetRequestJS>>();
					js.push(JS::This{});
					auto cb = std::make_shared<JS::CallbackMethod<string, NetResponseInfo>>(js);
					req->doneListeners.emplace_back(req->listener->request().OnRequestComplete.listen([cb](NetRequestPtr listener, const NetResponseInfo& info) {
						JS::StackAssert sa(cb->context());
						auto& js = cb->context();
						auto str = listener->data_string();
						(*cb)(str, info);
						js.pop();
						return true;
					}));
					return 1;
				}, 1}}
			)}
		)},
		// net.get
		JS::Property<JS::Function>{"get", JS::Function{[this](JS::Context& js) {
			JS::StackAssert sa(js, 1);
			NetRequestOptions opts;
			// first arg [string/object]
			if (js.is<string>(0)) {
				opts.url = js.get<string>(0);
				if (js.is<JS::Object>(1)) opts = js.get<JS::Object>(1);
			}
			else if (js.is<JS::Object>(0)) opts = js.get<JS::Object>(0);
			else js.raise(JS::ParameterError("expected string or object (idx: 0)"));

			auto listener = m_requests->add(opts);
			js.construct(JS::Shared<NetRequestJS>{std::make_shared<NetRequestJS>(listener)});
			js.push(JS::This{});
			js.get_global<void>("\xff""\xff""module-net");
			js.get_property<void>(-1, "Request");
			js.get_property<void>(-1, "prototype");
			js.set_prototype(-4);
			js.pop(2);
			return 1;
		}, 2}}
	);
	ctx.dup();
	ctx.put_global("\xff""\xff""module-net");
}