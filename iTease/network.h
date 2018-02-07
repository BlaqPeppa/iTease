#pragma once
#include <curl/curl.h>
#include "common.h"
#include "module.h"
#include "event.h"
#include "network_detail.h"

namespace iTease {
	class NetRequest;
	class NetMultiRequest;

	using NetRequestPtr = shared_ptr<NetRequest>;

	struct NetRequestOptions {	
		string url;
		bool progress = false;
		bool follow = true;
		optional<long> timeout;

		auto& operator=(const JS::VariantMap& vm) {
			if (auto ptr = map_find<JS::Variant>(vm, "url"))
				*ptr >> Convert<string>{url};
			if (auto ptr = map_find<JS::Variant>(vm, "progress"))
				*ptr >> Convert<bool>{progress};
			if (auto ptr = map_find<JS::Variant>(vm, "follow"))
				*ptr >> Convert<bool>{follow};
			if (auto ptr = map_find<JS::Variant>(vm, "timeout"))
				*ptr >> Convert<optional<long>>{timeout};
			return *this;
		}
	};
	
	class NetException : public std::runtime_error {
	public:
		NetException(const string& msg) : runtime_error(msg)
		{ }

		using std::exception::what;
	};
	class NetRequestException : public NetException {
	public:
		NetRequestException(CURLcode code) : NetException(curl_easy_strerror(code))
		{ }
		NetRequestException(const string& msg) : NetException(msg)
		{ }
	};
	class NetMultiException : public NetException {
	public:
		NetMultiException(CURLMcode code) : NetException(curl_multi_strerror(code))
		{ }
		NetMultiException(const string& msg) : NetException(msg)
		{ }
	};

	// CURL messages returned by get_info()
	class NetResponseInfo {
	public:
		static constexpr const char* JSName = "\xff""\xff""NetResponseInfo";

		NetResponseInfo(NetRequestPtr request, const CURLMsg* msg);
		NetResponseInfo(NetRequestPtr request, CURLcode code);

		inline operator bool() const { return ok(); }
		inline bool ok() const { return code == CURLE_OK; }

	public:
		const NetRequestPtr request;
		const CURLMSG message = CURLMSG_NONE;
		const void* data = nullptr;
		const CURLcode code = CURLE_OK;
	};

	class NetRequest : public enable_shared_from_this<NetRequest> {
		friend class NetResponseInfo;
		friend class NetMultiRequest;
		friend class Listener;

	public:
		using OnRequestCompleteEvent = Event<shared_ptr<NetRequest>, NetResponseInfo>;
		using OnWriteDataEvent = Event<shared_ptr<NetRequest>>;

		NetRequest(string url);
		NetRequest(NetRequestOptions opts);
		NetRequest(CURL* handle, NetRequestOptions opts);
		NetRequest(NetRequest&&);
		~NetRequest() NOEXCEPT;
		// use clone() to create a new identical request
		NetRequest(const NetRequest&) = delete;
		auto operator=(const NetRequest&)->NetRequest& = delete;
		// returns true if the objects represent the same request (not true for cloned requests)
		auto operator==(const NetRequest&) const->bool;

		// creates a new request with identical options to this one
		auto clone() const->shared_ptr<NetRequest>;
		// resets the request, preserving the options
		auto reset() NOEXCEPT->void;
		// pause the request, does not work for non-network requests (e.g. file://)
		auto pause(const int bimask)->void;
		// performs the request, blocking until completion/failure or timeout
		auto perform()->void;
		// returns true if the request has been fully performed
		auto finished() const { return m_finished; }
		// returns the written data
		auto& data() const { return m_data; }
		// returns the written data as a new string
		auto data_string() const { return string{m_data.begin(), m_data.end()}; }
		// returns and caches underlying cURL information obtained while resolving the request
		template<CURLINFO InfoID>
		auto info() const->detail::CURLInfo_t<InfoID> {
			detail::CURLInfo_t<InfoID> v;
			auto code = curl_easy_getinfo(m_curl, InfoID, &v);
			if (code != CURLE_OK) throw NetRequestException(code);
			return v;
		}

	public:
		NetRequestOptions opts;
		OnRequestCompleteEvent OnRequestComplete;
		OnWriteDataEvent OnWriteData;

	private:
		auto apply()->void;

		static auto write_func(char* d, size_t n, size_t l, void* p)->size_t;

	private:
		CURL* curl() const;

	private:
		CURL* m_curl = nullptr;
		bool m_applied = false;
		bool m_finished = false;
		vector<char> m_data;
		unordered_map<CURLINFO, variant<long, long long, double, string>> m_info;
	};

	// Manages multiple asynchronous NetRequest objects
	class NetMultiRequest : public enable_shared_from_this<NetMultiRequest> {
	public:
		friend class Listener;

		using HandleMap = unordered_map<CURL*, shared_ptr<NetRequest>>;

		// Listener class to remove unreferenced NetRequest pointers from the NetMultiRequest
		class Listener {
			friend NetMultiRequest;
			Listener(NetMultiRequest& multi, NetRequest* req) {
				observe(multi, req);
			}

		public:
			Listener() { }
			Listener(Listener&& other) {
				m_multi = other.m_multi;
				m_request = other.m_request;
				other.m_multi.reset();
			}
			~Listener() {
				reset();
			}
			Listener(const Listener& other) = delete;
			Listener& operator=(const Listener& other) = delete;
			Listener& operator=(Listener&& other) {
				reset();
				m_multi = other.m_multi;
				m_request = other.m_request;
				other.m_multi.reset();
				return *this;
			}
			
			inline operator bool() const { return !expired(); }

			inline bool expired() const { return m_multi.expired(); }
			NetResponseInfo info() const {
				assert(!expired());
				return m_multi.lock()->get_info(m_request).value();
			}
			NetRequest& request() const {
				assert(!expired());
				return *m_request;
			}

		private:
			void observe(NetMultiRequest& multi, NetRequest* req) {
				reset();
				m_multi = multi.shared_from_this();
				m_request = req;
			}
			void reset() {
				if (!m_multi.expired()) m_multi.lock()->remove(m_request);
				m_multi.reset();
			}

		private:
			weak_ptr<NetMultiRequest> m_multi;
			NetRequest* m_request;
		};

	public:
		NetMultiRequest();
		NetMultiRequest(NetMultiRequest&&);
		NetMultiRequest(const NetMultiRequest&) = delete;
		~NetMultiRequest() NOEXCEPT;
		auto operator=(const NetMultiRequest&)->NetMultiRequest& = delete;
		auto operator=(NetMultiRequest&&)->NetMultiRequest&;

		// add a new request to the queue, exists for the lifetime of the returned pointer
		auto add(NetRequestOptions)->shared_ptr<Listener>;
		// perform active requests, returns true if all are complete
		auto perform()->bool;
		// get information on all requests
		auto get_info()->vector<NetResponseInfo>;
		// returns true if the request has finished
		auto is_finished(shared_ptr<NetRequest> req)->bool;
		// returns the number of active requests after the last perform()
		auto num_active() const { return m_numActive; }

	private:
		// get information on a specific request, empty return if not found
		auto get_info(NetRequest*)->optional<NetResponseInfo>;
		// remove the request, done when the Listener goes out of scope
		auto remove(NetRequest*)->void;

	private:
		// deleter for the multi handle
		struct curl_multi_deleter {
			auto operator()(CURLM* ptr) const->void;
		};

		using curl_multi_ptr = unique_ptr<CURLM, curl_multi_deleter>;

	private:
		curl_multi_ptr m_curl = nullptr;
		int m_numActive = 0;
		int m_numQueued = 0;
		HandleMap m_handles;
	};

	using NetListener = NetMultiRequest::Listener;
	using NetListenerPtr = shared_ptr<NetListener>;

	struct NetRequestJS {
		static constexpr const char* JSName = "\xff""\xff""NetRequest";

		NetRequestJS(NetListenerPtr listener) : listener(listener)
		{ }

		NetListenerPtr listener;
		vector<NetRequest::OnRequestCompleteEvent::Listener> doneListeners;
	};

	class Network : public Module {
	public:
		Network(Application&);
		~Network();
		virtual auto init_module(Application&)->void override;
		virtual auto init_js(JS::Context&)->void override;

		auto run()->void;

	private:
		bool m_init = false;
		Application& m_app;
		Application::OnTickEvent::Listener m_onTick;
		shared_ptr<NetMultiRequest> m_requests;
	};
	
	namespace JS {
		template<>
		class TypeInfo<NetRequestJS> {
		public:
			static void apply(Context& js, NetRequestJS& reqjs) {
				auto listener = reqjs.listener;
				auto req = listener->request().shared_from_this();
				js.put_properties(-1,
					Property<string>{"url", JS::Function{[req](JS::Context& js) {
						js.push(req->opts.url);
						return 1;
					}, 0}, JS::Function{}},
					Property<unsigned int>{"timeout", JS::Function{[req](JS::Context& js) {
						js.push<unsigned int>(req->opts.timeout.value_or(0));
						return 1;
					}, 0}, JS::Function{}},
					Property<bool>{"finished", JS::Function{[req](JS::Context& js) {
						js.push(req->finished());
						return 1;
					}, 0}, JS::Function{}},
					Property<string>{"data", JS::Function{[req](JS::Context& js) {
						js.push(req->data_string());
						return 1;
					}, 0}, JS::Function{}}
				);
			}
			static void push(Context& js, NetRequestJS& req) {
				StackAssert sa(js, 1);
				js.push_object();
				apply(js, std::move(req));
				js.pop();
			}
			static void construct(Context& js, NetRequestJS& req) {
				js.push_this_object();
				apply(js, std::move(req));
				js.pop();
			}
		};
	}

	namespace JS {
		template<>
		class TypeInfo<NetResponseInfo> {
		public:
			static void apply(Context& js, const NetResponseInfo& info) {
				StackAssert sa(js);
				auto request = info.request;
				js.put_properties(-1,
					Property<int>{"status", JS::Function{[request](JS::Context& js){
						js.push<int>(request->info<CURLINFO_RESPONSE_CODE>());
						return 1;
					}, 0}, JS::Function{}},
					Property<double>{"size", JS::Function{[request](JS::Context& js) {
						js.push(static_cast<double>(request->info<CURLINFO_SIZE_DOWNLOAD_T>()));
						return 1;
					}, 0}, JS::Function{}},
					Property<double>{"speed", JS::Function{[request](JS::Context& js) {
						js.push<double>(request->info<CURLINFO_SPEED_DOWNLOAD_T>());
						return 1;
					}, 0}, JS::Function{}},
					Property<double>{"upload_size", JS::Function{[request](JS::Context& js) {
						js.push<double>(request->info<CURLINFO_SIZE_UPLOAD_T>());
						return 1;
					}, 0}, JS::Function{}},
					Property<unsigned int>{"request_size", JS::Function{[request](JS::Context& js) {
						js.push<unsigned int>(request->info<CURLINFO_REQUEST_SIZE>());
						return 1;
					}, 0}, JS::Function{}},
					Property<string>{"last_url", JS::Function{[request](JS::Context& js) {
						js.push<const char*>(request->info<CURLINFO_EFFECTIVE_URL>());
						return 1;
					}, 0}, JS::Function{}},
					Property<double>{"filetime", JS::Function{[request](JS::Context& js) {
						js.push<double>(request->info<CURLINFO_FILETIME>());
						return 1;
					}, 0}, JS::Function{}},
					Property<int>{"redirect_count", JS::Function{[request](JS::Context& js) {
						js.push<int>(request->info<CURLINFO_REDIRECT_COUNT>());
						return 1;
					}, 0}, JS::Function{}},
					Property<double>{"redirect_time", JS::Function{[request](JS::Context& js) {
						js.push<double>(request->info<CURLINFO_REDIRECT_TIME>());
						return 1;
					}, 0}, JS::Function{}},
					Property<double>{"start_time", JS::Function{[request](JS::Context& js) {
						js.push(request->info<CURLINFO_STARTTRANSFER_TIME>());
						return 1;
					}, 0}, JS::Function{}},
					Property<double>{"total_time", JS::Function{[request](JS::Context& js) {
						js.push(request->info<CURLINFO_TOTAL_TIME>());
						return 1;
					}, 0}, JS::Function{}}
				);
			}
			static void push(Context& js, const NetResponseInfo& info) {
				StackAssert sa(js, 1);
				js.push_object();
				apply(js, std::move(info));
			}
			static void construct(Context& js, const NetResponseInfo& info) {
				StackAssert sa(js);
				js.push_this_object();
				apply(js, std::move(info));
				js.pop();
			}
		};
	}
}