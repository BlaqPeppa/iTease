#pragma once
#include <duktape/duk_config.h>
#include <duktape/duktape.h>
//#include <duktape/duk_debug.h>
//#include <duktape/duk_exception.h>
#include <duktape/duk_module_node.h>
#include "common.h"
#include "logging.h"

namespace iTease {
	namespace JS {
		class Context;

		template<typename T>
		class TypeInfo { };
		class Array { };
		class Global { };
		class GlobalStash { };
		class Undefined { };
		class Null { };
		class This { };
		class Function {
		public:
			Function() = default;
			Function(std::function<int(Context&)>&& func, int n = 0) : function(func), nargs(n) { }
			inline operator bool() const { return (bool)function; }
			std::function<int(Context&)> function;
			int nargs = 0;
		};
		template<typename T> struct RawPointer {
			T* object;
		};
		template<typename T> struct Shared {
			shared_ptr<T> object;
		};
		template<typename T> struct Pointer {
			T* object;
		};

		using Variant = any;
		using VariantVector = vector<Variant>;
		using VariantMap = map<string, Variant>;

		enum class PointerType {
			RawPointer, SharedPointer, Pointer
		};

		struct VarPointer {
			PointerType type;
			void* ptr;
		};

		template<typename T, typename Tgetter = Function, typename Tsetter = Tgetter>
		class Property {
		public:
			Property() = default;
			Property(string n, const T& v, int f = DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_ENUMERABLE | DUK_DEFPROP_SET_CONFIGURABLE) :
				name(n), value(v), flags(f | DUK_DEFPROP_HAVE_VALUE)
			{  }
			Property(string n, T&& v, int f = DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_ENUMERABLE | DUK_DEFPROP_SET_CONFIGURABLE) :
				name(n), value(v), flags(f | DUK_DEFPROP_HAVE_VALUE)
			{  }
			Property(string n, Tgetter g, Tsetter s, int f = DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_ENUMERABLE | DUK_DEFPROP_SET_CONFIGURABLE) :
				name(n), flags(f), getter(g), setter(s)
			{ }
			string name;
			T value;
			int flags = DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_ENUMERABLE | DUK_DEFPROP_SET_CONFIGURABLE;
			Tgetter getter = Tgetter();
			Tsetter setter = Tsetter();
		};
		template<typename... Targs>
		class PropertyList {
		public:
			std::initializer_list<Targs...> args;
		};
		class Object {
		public:
			Object() = default;
		};

		struct ErrorInfo {
		public:
			string name;
			string message;
			string stack;
			string fileName;
			int lineNumber = 0;
		};

		class Exception : public std::exception {
		public:
			Exception() = default;
			Exception(const char* err) : std::exception(err)
			{ }
		};
		class ErrorException : public Exception {
		public:
			ErrorException(Context& ctx, int code = 0, bool pop = true);
			template<typename T>
			ErrorException& operator<<(T rhs) {
				std::stringstream ss;
				ss << m_msg << rhs;
				m_msg = ss.str();
				return *this;
			}
			virtual const char* what() const noexcept override {
				return m_msg.c_str();
			}

		protected:
			string m_msg;
		};

		class ExceptionInfo : public std::runtime_error {
		public:
			ExceptionInfo(const char* msg = "unknown exception") noexcept : runtime_error(msg), m_msg(msg) { }
			ExceptionInfo(ErrorInfo* info) noexcept : m_info(info), runtime_error("javascript error")
			{ }

			inline const char* message() const noexcept { return m_msg; }

			virtual const char* what() const noexcept override {
				static char buff[4086];
				if (m_info)
					std::sprintf(buff, "*** ERROR: %s: %s\n\r*** FILE: %s:%d\n\r***STACK: %s", m_info->name.c_str(), m_info->message.c_str(), m_info->fileName.c_str(), m_info->lineNumber, m_info->stack.c_str());
				else
					std::sprintf(buff, "*** FATAL ERROR: %s", m_msg);
				return buff;
			}

		private:
			const char* m_msg;
			ErrorInfo* m_info = nullptr;
		};

		class Error {
		public:
			inline Error(string message) noexcept : m_name("Error"), m_message(std::move(message)) { }
			inline const string& Name() const noexcept {
				return m_name;
			}

			virtual void create(Context& ctx) const noexcept;

		protected:
			string m_name;
			string m_message;

			inline Error(string name, string message) noexcept : m_name(std::move(name)), m_message(std::move(message)) { }
		};
		// Invalid parameter
		class ParameterError : public Error {
		public:
			inline ParameterError(string message) noexcept : Error("Error", std::move(message)) { }
		};
		// Error in eval() function
		class EvalError : public Error {
		public:
			inline EvalError(string message) noexcept : Error("EvalError", std::move(message)) { }
		};
		// Value is out of range
		class RangeError : public Error {
		public:
			inline RangeError(string message) noexcept : Error("RangeError", std::move(message)) { }
		};
		// Trying to use a variable that does not exist
		class ReferenceError : public Error {
		public:
			inline ReferenceError(string message) noexcept : Error("ReferenceError", std::move(message))
			{ }
		};
		// Syntax error in the script
		class SyntaxError : public Error {
		public:
			inline SyntaxError(string message) noexcept : Error("SyntaxError", std::move(message))
			{ }
		};
		// Invalid type given
		class TypeError : public Error {
		public:
			inline TypeError(string message) noexcept : Error("TypeError", std::move("expected a " + message))
			{ }
		};
		// URI manipulation failure
		class URIError : public Error {
		public:
			inline URIError(string message) noexcept : Error("URIError", std::move(message))
			{ }
		};

		class Context {
			using ContextPtr = duk_context*;
			using Deleter = void(*)(ContextPtr);
			using Handle = unique_ptr<duk_context, Deleter>;

			Context(const Context&) = delete;
			Context(const Context&&) = delete;
			Context& operator=(const Context&) = delete;
			Context& operator=(const Context&&) = delete;

		public:
			Context() : m_handle(duk_create_heap(
				// malloc
				[](void* udata, duk_size_t size) {
					return malloc(size);
				},
				// realloc
				[](void* udata, void* ptr, duk_size_t size) {
					return realloc(ptr, size);
				},
				// free
				[](void* udata, void* ptr) {
					return free(ptr);
				},
				// heap data
				this,
				// fatal handler
				[](void *udata, const char *msg) {
					throw Exception(msg);
				}
			), duk_destroy_heap)
			{ }
			inline Context(ContextPtr ctx) noexcept : m_handle(ctx, [](ContextPtr) { }) { }
			inline operator duk_context*() noexcept { return m_handle.get(); }
			inline operator duk_context*() const noexcept { return m_handle.get(); }

			inline bool is_constructor_call() const { return duk_is_constructor_call(*this); }
			inline void create(unsigned nargs = 0) { duk_new(*this, nargs); }
			inline void call(unsigned nargs = 0) { duk_call(*this, nargs); }
			//template<typename Tfun, typename... Targs>
			//inline void call(const Tfun& func, Targs... args) { dukglue_call(*this, func, args); }
			inline void copy(int from, int to) { duk_copy(*this, from, to); }
			inline void define_property(int idx, int flags) { duk_def_prop(*this, idx, flags); }
			inline bool delete_property(int idx) { return duk_del_prop(*this, idx); }
			inline bool delete_property(int idx, int prop) { return duk_del_prop_index(*this, idx, prop); }
			inline bool delete_property(int idx, const string& name) { return duk_del_prop_string(*this, idx, name.c_str()); }
			inline void dup() { duk_dup_top(*this); }
			inline void dup(int idx) { duk_dup(*this, idx); }
			inline void eval() { duk_eval(*this); }
			inline bool has_property(int idx) { return duk_has_prop(*this, idx); }
			inline bool has_property(int idx, int position) { return duk_has_prop_index(*this, idx, position); }
			inline bool has_property(int idx, const string& name) { return duk_has_prop_string(*this, idx, name.c_str()); }
			inline bool instanceof(int idx1, int idx2) { return duk_instanceof(*this, idx1, idx2); }
			inline void insert(int to) { duk_insert(*this, to); }
			inline void pop(unsigned count = 1) { duk_pop_n(*this, count); }
			inline void remove(int idx) { duk_remove(*this, idx); }
			inline void replace(int idx) { duk_replace(*this, idx); }
			inline void swap(int idx1, int idx2) { duk_swap(*this, idx1, idx2); }
			inline void set_length(int idx, int length) { duk_set_length(*this, idx, length); }
			inline int top() const noexcept { return duk_get_top(*this); }
			inline int top_index() const noexcept { return duk_get_top_index(*this); }
			inline int normalize_index(int idx) const noexcept { return duk_normalize_index(*this, idx); }
			inline int type(int idx) const noexcept { return duk_get_type(*this, idx); }
			inline int length(int idx) const noexcept { return duk_get_length(*this, idx); }
			inline bool to_boolean(int idx) const noexcept { return duk_to_boolean(*this, idx); }
			inline void prototype(int idx) const noexcept { return duk_get_prototype(*this, idx); }
			inline void set_prototype(int idx) const noexcept { return duk_set_prototype(*this, idx); }
			inline void push_current_function() const noexcept { return duk_push_current_function(*this); }
			inline string encode(int index) const noexcept {
				auto ptr = duk_json_encode(*this, index);
				return ptr ? ptr : "";
			}
			inline int decode(int idx) const noexcept { duk_json_decode(*this, idx); }
			inline int decode(int idx, string str) const noexcept {
				duk_push_string(*this, str.c_str());
				duk_json_decode(*this, idx);
			}
			template<typename Tfunc>
			void enumerate(int idx, int flags, bool get_value, Tfunc func) {
				duk_enum(*this, idx, flags);
				while (duk_next(*this, -1, get_value)) {
					func(get<string>(get_value ? -2 : -1));
					pop(get_value ? 2 : 1);
				}
				pop();
			}
			inline void raise() { duk_throw(*this); }
			template<typename Texception>
			void raise(const Texception& ex) {
				ex.create(*this);
				raise();
				throw ex;
			}
			inline void pcall(unsigned nargs = 0) {
				auto code = duk_pcall(*this, nargs);
				if (code) {
					throw ErrorException(*this, code);
				}
			}
			template<typename U>
			inline void eval(U&& source) { source.eval(*this); }
			void peval() {
				auto code = duk_peval(*this);
				if (code) {
					throw ErrorException(*this, code);
				}
			}

			template<typename T>
			inline void push(const T& value) {
				TypeInfo<typename std::remove_const<std::remove_reference<T>::type>::type>::push(*this, value);
			}
			template<typename T>
			inline void push(T& value) {
				TypeInfo<typename std::remove_const<std::remove_reference<T>::type>::type>::push(*this, value);
			}
			template<typename T, typename... Targs>
			void push(T& arg, Targs&&... args) {
				TypeInfo<T>::push(*this, arg);
				push(args...);
			}
			
			template<typename Tret, typename Tsrc>
			inline typename std::enable_if<std::is_void<Tret>::value, Tret>::type peval(Tsrc&& source) {
				int prev_top = top();
				auto code = TypeInfo<Tsrc>::peval(*this, source);
				if (code != 0)
					throw ErrorException(*this, code);
				pop(top() - prev_top);
			}
			template<typename Tret, typename Tsrc>
			inline typename std::enable_if<!std::is_void<Tret>::value, Tret>::type peval(Tsrc&& source) {
				int prev_top = Top();
				Tret ret;
				push(source);
				push((void*)&ret);
				int code = duk_safe_call(*this, &dukglue::detail::eval_safe<Tret>, 2, 1);
				if (code != 0) {
					print_dump();
					throw ErrorException(*this, code);
				}
				pop(Top() - prev_top);
				return ret;
			}
			void call_method(int nargs) {
				duk_call_method(*this, nargs);
			}
			void pcall_method(unsigned nargs) {
				auto rc = duk_pcall_method(*this, nargs);
				if (rc != 0)
					throw(ErrorException(*this, rc));
			}

			template<typename T>
			inline auto get(int index) -> decltype(TypeInfo<T>::get(*this, 0)) {
				return TypeInfo<T>::get(*this, index);
			}

			template<typename T>
			inline auto require(int index) -> decltype(TypeInfo<T>::require(*this, 0)) {
				#ifdef DUK_USE_CPP_EXCEPTIONS
				try {
				#endif
					return TypeInfo<T>::require(*this, index);
				#ifdef DUK_USE_CPP_EXCEPTIONS
				}
				catch (duk_internal_exception) {
					throw ErrorException(*this);
				}
				#endif
			}
			template<typename T>
			inline bool is(int index) {
				return TypeInfo<T>::is(*this, index);
			}
			template<typename Type, typename DefaultValue>
			auto optional(int index, DefaultValue&& defaultValue) {
				return TypeInfo<std::decay_t<Type>>::optional(*this, index, std::forward<DefaultValue>(defaultValue));
			}
			template<typename Type, typename std::enable_if_t<!std::is_void<Type>::value>* = nullptr>
			auto get_property(int index, const string& name) -> decltype(get<Type>(0)) {
				duk_get_prop_string(*this, index, name.c_str());
				decltype(get<Type>(0)) value = get<Type>(-1);
				pop();
				return value;
			}
			template<typename Type, typename DefaultValue>
			auto optional_property(int index, const string& name, DefaultValue&& def) -> decltype(optional<Type>(0, std::forward<DefaultValue>(def))) {
				duk_get_prop_string(*this, index, name.c_str());
				decltype(optional<Type>(0, std::forward<DefaultValue>(def))) value = optional<Type>(-1, std::forward<DefaultValue>(def));
				pop();
				return value;
			}
			template<typename Type, typename std::enable_if_t<!std::is_void<Type>::value>* = nullptr>
			auto get_property(int index, int position) -> decltype(get<Type>(0)) {
				duk_get_prop_index(*this, index, position);
				decltype(get<Type>(0)) value = get<Type>(-1);
				pop();
				return value;
			}
			template<typename Type, typename DefaultValue>
			auto optional_property(int index, int position, DefaultValue&& def) -> decltype(optional<Type>(0, std::forward<DefaultValue>(def))) {
				duk_get_prop_index(m_handle.get(), index, position);
				decltype(optional<Type>(0, std::forward<DefaultValue>(def))) value = optional<Type>(-1, std::forward<DefaultValue>(def));
				pop();
				return value;
			}

			template<typename Type, typename std::enable_if_t<std::is_void<Type>::value>* = nullptr>
			void get_property(int index, const string& name) {
				duk_get_prop_string(m_handle.get(), index, name.c_str());
			}
			template<typename Type, typename std::enable_if_t<std::is_void<Type>::value>* = nullptr>
			void get_property(int index, int position) {
				duk_get_prop_index(*this, index, position);
			}
			template<typename Type>
			void put_property(int index, const string& name, Type&& value) {
				index = duk_normalize_index(m_handle.get(), index);
				push(value);
				duk_put_prop_string(*this, index, name.c_str());
			}
			template<typename Type>
			void put_property(int index, int position, Type&& value) {
				index = duk_normalize_index(*this, index);
				push(std::forward<Type>(value));
				duk_put_prop_index(*this, index, position);
			}
			template<typename T>
			void put_property(int idx, Property<T>&& val) {
				idx = duk_normalize_index(*this, idx);
				TypeInfo<Property<T>>::put_property(*this, idx, val);
			}
			void put_properties(int idx) { }
			template<typename Targ>
			void put_properties(int idx, Targ arg) {
				put_property(idx, std::forward<Targ>(arg));
			}
			template<typename Targ, typename... Targs>
			void put_properties(int idx, Targ arg, Targs&&... args) {
				//StackAssert sa(*this); // stack may be reduced by the number of arguments that represent objects already on stack
				put_property(idx, std::forward<Targ>(arg));
				put_properties(idx, args...);
			}
			void put_property(int idx, const string& name) {
				duk_put_prop_string(*this, idx, name.c_str());
			}
			void put_property(int idx, int pos) {
				duk_put_prop_index(*this, idx, pos);
			}
			template<typename Type>
			auto get_global(const string& name, std::enable_if_t<!std::is_void<Type>::value>* = nullptr) -> decltype(get<Type>(0)) {
				duk_get_global_string(*this, name.c_str());
				decltype(get<Type>(0)) value = get<Type>(-1);
				duk_pop(*this);
				return value;
			}
			template<typename Type>
			void get_global(const string& name, std::enable_if_t<std::is_void<Type>::value>* = nullptr) noexcept {
				duk_get_global_string(*this, name.c_str());
			}
			template<typename Type>
			void put_global(const string& name, Type&& type) {
				push(std::forward<Type>(type));
				duk_put_global_string(m_handle.get(), name.c_str());
			}
			void put_global(const string& name) {
				duk_put_global_string(*this, name.c_str());
			}
			template<typename... Targs>
			auto push_object(Targs&&... args) {
				//StackAssert sa(*this, 1);
				push(Object{ });
				put_properties(-1, args...);
				return Object{ };
			}
			template<typename... Targs>
			auto push_this_object(Targs&&... args) {
				push(This{ });
				put_properties(-1, args...);
				return Object{ };
			}
			template<typename... Targs>
			auto push_function_object(Function fn, Targs&&... args) {
				push(fn);
				put_properties(-1, args...);
				return Object{ };
			}
			template<typename... Targs>
			void put_global_object(const string& name, Targs&&... properties) {
				push_object(properties...);
				put_global(name);
			}
			template<typename... Targs>
			void set_global_object_props(const string& name, Targs&&... properties) {
				get_global<void>(name);
				put_properties(properties...);
				pop();
			}
			template<typename... Targs>
			void put_global_stash_props(Targs&&... properties) {
				push(GlobalStash{ });
				put_properties(-1, properties...);
				pop();
			}
			template<typename T>
			inline void construct(T&& value) {
				TypeInfo<std::decay_t<T>>::construct(*this, std::forward<T>(value));
			}
			template<typename T>
			inline auto self() -> decltype(TypeInfo<T>::get(*this, 0)) {
				duk_push_this(*this);
				decltype(TypeInfo<T>::get(*this, 0)) value = TypeInfo<T>::get(*this, -1);
				pop();
				return value;
			}
			#ifndef NDEBUG
			// Prefer print_dump()...
			template<typename T>
			inline auto dump(std::enable_if_t<!std::is_void<T>::value>* = nullptr) -> decltype(get<T>(0)) {
				duk_push_context_dump(*this);
				auto value = get<T>(-1);
				duk_pop(*this);
				return value;
			}
			// Prefer print_dump()...
			template<typename T>
			inline void dump(std::enable_if_t<std::is_void<T>::value>* = nullptr) noexcept {
				duk_push_context_dump(*this);
			}
			#endif

			template<typename T = string>
			inline void print_dump() {
				#ifndef NDEBUG
				Debug() << dump<T>();
				//ITEASE_LOGDEBUG(dump<T>());
				#endif
			}

			void copy_properties(int srcIdx, int destIdx, int flags = 0) noexcept;

			void init_module_loader();
			auto require_module(string_view sv) {
				get_global<void>("require");
				push(sv.data());
				pcall(1);
			}

		private:
			Handle m_handle;
		};

		class StackAssert {
		public:
			// create the stack checker. No-op if NDEBUG is set.
			inline StackAssert(Context& ctx, int expected = 0) noexcept
				#if !defined(NDEBUG)
				: m_context(ctx)
				, m_expected(expected)
				, m_begin(static_cast<unsigned>(m_context.top()))
				#endif
			{
				#if defined(NDEBUG)
				(void)ctx;
				(void)expected;
				#endif
			}
			// Verify the expected size. No-op if NDEBUG is set.
			inline ~StackAssert() noexcept {
				#if !defined(NDEBUG)
				unsigned t = m_context.top();
				bool b = t - m_begin == m_expected;
				if (!std::uncaught_exceptions())
					assert(b);
				#endif
			}

			#if !defined(NDEBUG)
		private:
			Context& m_context;
			int m_expected;
			unsigned m_begin;
			#endif
		};

		template<> class TypeInfo<bool> {
		public:
			static inline bool get(Context& ctx, int index) { return duk_get_boolean(ctx, index); }
			static inline bool is(Context& ctx, int index) { return duk_is_boolean(ctx, index); }
			static inline bool optional(Context& ctx, int index, bool defaultValue) { return duk_is_boolean(ctx, index) ? duk_get_boolean(ctx, index) : defaultValue; }
			static inline void push(Context& ctx, bool value) { duk_push_boolean(ctx, value); }
			static inline bool require(Context& ctx, int index) { return duk_require_boolean(ctx, index); }
		};
		template<> class TypeInfo<int> {
		public:
			static inline int get(Context& ctx, int index) { return duk_get_int(ctx, index); }
			static inline bool is(Context& ctx, int index) { return duk_is_number(ctx, index); }
			static inline int optional(Context& ctx, int index, int defaultValue) { return duk_is_number(ctx, index) ? duk_get_int(ctx, index) : defaultValue; }
			static inline void push(Context& ctx, int value) { duk_push_int(ctx, value); }
			static inline int require(Context& ctx, int index) { return duk_require_int(ctx, index); }
		};
		template<> class TypeInfo<unsigned int> {
		public:
			static inline int get(Context& ctx, int index) { return duk_get_uint(ctx, index); }
			static inline bool is(Context& ctx, int index) { return duk_is_number(ctx, index); }
			static inline int optional(Context& ctx, int index, int defaultValue) { return duk_is_number(ctx, index) ? duk_get_uint(ctx, index) : defaultValue; }
			static inline void push(Context& ctx, int value) { duk_push_uint(ctx, value); }
			static inline int require(Context& ctx, int index) { return duk_require_uint(ctx, index); }
		};
		template<> class TypeInfo<double> {
		public:
			static inline double get(Context& ctx, int index) { return duk_get_number(ctx, index); }
			static inline bool is(Context& ctx, int index) { return duk_is_number(ctx, index); }
			static inline double optional(Context& ctx, int index, double defaultValue) { return duk_is_number(ctx, index) ? duk_get_number(ctx, index) : defaultValue; }
			static inline void push(Context& ctx, double value) { duk_push_number(ctx, value); }
			static inline double require(Context& ctx, int index) { return duk_require_number(ctx, index); }
		};
		template<> class TypeInfo<string> {
		public:
			static inline string get(Context& ctx, int index) {
				duk_size_t size;
				const char* text = duk_get_lstring(ctx, index, &size);
				return string{text, size};
			}
			static inline bool is(Context& ctx, int index) {
				return duk_is_string(ctx, index);
			}
			static inline string optional(Context& ctx, int index, string defaultValue) {
				if (!duk_is_string(ctx, index))
					return defaultValue;
				return get(ctx, index);
			}
			static inline void push(Context& ctx, const string& value) {
				duk_push_lstring(ctx, value.c_str(), value.length());
			}
			static inline string require(Context &ctx, int index) {
				duk_size_t size;
				auto text = duk_require_lstring(ctx, index, &size);
				return string{text, size};
			}
			static inline duk_ret_t peval(Context& ctx, const string& value) {
				//ctx.peval()
				return duk_peval_lstring(ctx, value.c_str(), value.size());
			}
		};
		template<> class TypeInfo<const char*> {
		public:
			static inline const char* get(Context& ctx, int index) { return duk_get_string(ctx, index); }
			static inline bool is(Context& ctx, int index) { return duk_is_string(ctx, index); }
			static inline const char* optional(Context& ctx, int index, const char* defVal) { return duk_is_string(ctx, index) ? duk_get_string(ctx, index) : defVal; }
			static inline void push(Context& ctx, const char* value) { duk_push_string(ctx, value); }
			static inline const char* require(Context& ctx, int index) { return duk_require_string(ctx, index); }
		};
		template<> class TypeInfo<Object> {
		public:
			static inline bool is(Context& ctx, int index) { return duk_is_object(ctx, index); }
			static inline void push(Context& ctx, const Object& obj) { duk_push_object(ctx); }
			static VariantMap get(Context& ctx, int idx);
		};
		template<> class TypeInfo<Array> {
		public:
			static inline bool is(Context& ctx, int index) { return duk_is_array(ctx, index); }
			static inline void push(Context& ctx, const Array &) { duk_push_array(ctx); }
			static VariantVector get(Context& ctx, int idx);
		};
		template<> class TypeInfo<Undefined> {
		public:
			static inline bool is(Context& ctx, int index) { return duk_is_undefined(ctx, index); }
			static inline void push(Context& ctx, const Undefined&) { duk_push_undefined(ctx); }
		};
		template<> class TypeInfo<Null> {
		public:
			static inline bool is(Context& ctx, int index) { return duk_is_null(ctx, index); }
			static inline void push(Context& ctx, const Null&) { duk_push_null(ctx); }
		};
		template<> class TypeInfo<This> {
		public:
			static inline void push(Context& ctx, const This&) { duk_push_this(ctx); }
		};
		template<> class TypeInfo<Global> {
		public:
			static inline void push(Context& ctx, const Global&) { duk_push_global_object(ctx); }
		};
		template<> class TypeInfo<GlobalStash> {
		public:
			static inline void push(Context& ctx, const GlobalStash&) { duk_push_global_stash(ctx); }
		};
		template<> class TypeInfo<Function> {
		public:
			static inline bool is(Context& ctx, int index) { return duk_is_function(ctx, index); }
			static inline void push(Context& ctx, Function fn) {
				// push function wrapper constructor
				duk_push_c_function(ctx, [](duk_context *ctx) -> duk_ret_t {
					Context context(ctx);

					duk_push_current_function(ctx);

					duk_get_prop_string(ctx, -1, "\xff""\xff""js-func");
					Function *f = static_cast<Function *>(duk_to_pointer(ctx, -1));
					duk_pop_2(ctx);
					if (f) {
						return static_cast<duk_ret_t>(f->function(context));
					}
					return 0;
				}, fn.nargs);

				// Store moved function as property
				duk_push_pointer(ctx, new Function(std::move(fn)));
				duk_put_prop_string(ctx, -2, "\xff""\xff""js-func");

				// Store deletion flag as property
				duk_push_boolean(ctx, false);
				duk_put_prop_string(ctx, -2, "\xff""\xff""js-deleted");

				// push and set the finalizer
				duk_push_c_function(ctx, [](duk_context *ctx) {
					duk_get_prop_string(ctx, 0, "\xff""\xff""js-deleted");

					if (!duk_to_boolean(ctx, -1)) {
						duk_push_boolean(ctx, true);
						duk_put_prop_string(ctx, 0, "\xff""\xff""js-deleted");
						duk_get_prop_string(ctx, 0, "\xff""\xff""js-func");
						delete static_cast<Function*>(duk_to_pointer(ctx, -1));
						duk_push_pointer(ctx, nullptr);
						duk_put_prop_string(ctx, 0, "\xff""\xff""js-func");
						duk_pop(ctx);
					}

					duk_pop(ctx);
					return 0;
				}, 1);
				duk_set_finalizer(ctx, -2);
			}
		};
		template<> class TypeInfo<Variant> {
		public:
			static void push(Context& ctx, const Variant&);
		};
		template<typename T>
		class TypeInfo<vector<T>> {
		public:
			static void push(Context& ctx, const vector<T>& vec) {
				ctx.push(Array{});
				int i = 0;
				for (auto& el : vec) {
					ctx.put_property(-1, i++, el);
				}
			}
		};
		template<typename T>
		class TypeInfo<map<string, T>> {
		public:
			static void push(Context& ctx, const map<string, T>& vec) {
				ctx.push(Object{});
				int i = 0;
				for (auto& el : vec) {
					ctx.put_property(-1, el.first, el.second);
				}
			}
		};
		template<typename T>
		class TypeInfo<Property<T>> {
		public:
			static void put_property(Context& ctx, int idx, Property<T>& v) {
				StackAssert sa(ctx);
				if (v.flags & (DUK_DEFPROP_HAVE_WRITABLE | DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_HAVE_GETTER)) {
					int flags = v.flags | (v.getter || v.setter ? 0 : DUK_DEFPROP_HAVE_VALUE);
					ctx.push(v.name);
					if (flags & DUK_DEFPROP_HAVE_VALUE) {
						ctx.push(v.value);
					}
					else {
						flags &= ~(DUK_DEFPROP_HAVE_WRITABLE);
						if (v.getter) {
							ctx.push(v.getter);
							flags |= DUK_DEFPROP_HAVE_GETTER;
						}
						if (v.setter) {
							ctx.push(v.setter);
							flags |= DUK_DEFPROP_HAVE_SETTER;
						}
					}
					ctx.define_property(idx, flags);
				}
				else ctx.put_property(idx, v.name, v.value);
			}
		};
		template<>
		class TypeInfo<Property<Object>> {
		public:
			static void put_property(Context& ctx, int idx, Property<Object>& v) {
				StackAssert sa(ctx, -1);
				ctx.swap(idx, idx - 1);
				unsigned flags = (v.getter || v.setter ? (v.flags & ~DUK_DEFPROP_HAVE_VALUE) : v.flags);
				ctx.push(v.name);
				if (flags & DUK_DEFPROP_HAVE_VALUE)
					ctx.dup(idx);
				ctx.remove(idx);

				if (v.getter) {
					ctx.push(v.getter);
					flags |= DUK_DEFPROP_HAVE_GETTER;
				}
				if (v.setter) {
					ctx.push(v.setter);
					flags |= DUK_DEFPROP_HAVE_SETTER;
				}

				ctx.get<string>(0);
				ctx.define_property(idx - 1, flags);
				//else ctx.put_property(idx - 1, v.name);
			}
		};
		template <typename T>
		class TypeInfo<Pointer<T>> {
		private:
			static void apply(Context& js, T* value) {;
				js.put_property(-1, "\xff""\xff""js-ptr", RawPointer<T>{ value });
				js.put_property(-1, "\xff""\xff""js-deleted", false);

				duk_push_c_function(js, [](duk_context* ctx)->duk_ret_t {
					Context js(ctx);
					if (!js.get_property<bool>(0, "\xff""\xff""js-deleted")) {
						delete js.get_property<RawPointer<T>>(0, "\xff""\xff""js-ptr");
						js.put_property(0, "\xff""\xff""js-deleted", true);
					}
					return 0;
				}, 1);
				duk_set_finalizer(js, -2);
			}

		public:
			static void construct(Context& ctx, Pointer<T> value) {
				ctx.push(This{ });
				apply(ctx, value.object);
				ctx.pop();
			}
			// push a managed pointer as object
			static void push(Context& ctx, Pointer<T> value) {
				StackAssert sa(ctx, 1);
				ctx.push(Object{ });
				apply(ctx, value.object);
				value.object->prototype(ctx);
				duk_set_prototype(ctx, -2);
			}
			// get a managed pointer from the stack. Pointer may be deleted at any time.
			static T* get(Context& ctx, int index) {
				duk_get_prop_string(ctx, index, T::JSName);

				if (duk_get_type(ctx, -1) == DUK_TYPE_UNDEFINED) {
					duk_pop(ctx);
					ctx.raise(ReferenceError("invalid this binding"));
				} else {
					duk_pop(ctx);
				}

				duk_get_prop_string(ctx, index, "\xff""\xff""js-ptr");
				T* value = static_cast<T*>(duk_to_pointer(ctx, -1));
				duk_pop(ctx);
				return value;
			}
		};
		template<typename T>
		class TypeInfo<RawPointer<T>> {
		public:
			static inline T* get(Context& ctx, int idx) { return static_cast<T*>(duk_to_pointer(ctx, idx)); }
			static inline bool is(Context& ctx, int idx) { return duk_is_pointer(ctx, idx); }
			static inline T* optional(Context& ctx, int idx, RawPointer<T> defaultValue) {
				if (!duk_is_pointer(ctx, idx)) return defaultValue.object;
				return static_cast<T*>(duk_to_pointer(ctx, idx));
			}
			static inline void push(Context& ctx, const RawPointer<T>& value) {
				duk_push_pointer(ctx, value.object);
			}
			static inline T* require(Context& ctx, int idx) {
				return static_cast<T*>(duk_require_pointer(ctx, idx));
			}
		};
		template <typename T>
		class TypeInfo<Shared<T>> {
			// allow shared ptr construction of object for native code
			static inline void apply_ptr(Context& ctx, shared_ptr<T> value) {
				ctx.put_property(-1, "\xff""\xff""js-deleted", false);
				ctx.put_property(-1, "\xff""\xff""js-shared-ptr", RawPointer<shared_ptr<T>>{new shared_ptr<T>(std::move(value))});
				duk_push_c_function(ctx, [](duk_context* ctx)->duk_ret_t {
					Context js(ctx);
					if (!js.get_property<bool>(0, "\xff""\xff""js-deleted")) {
						js.put_property(0, "\xff""\xff""js-deleted", true);
						delete js.get_property<RawPointer<shared_ptr<T>>>(0, "\xff""\xff""js-shared-ptr");
					}
					return 0;
				}, 1);
				duk_set_finalizer(ctx, -2);
			}
			template<typename U = T>
			static void apply(Context& ctx, shared_ptr<U> value) {
				apply_ptr(ctx, value);
			}
			// allow full object construction for native code, and JS code via TypeInfo<T>::apply(Context&, shared_ptr<T> value)
			template<typename U = T>
			static auto apply(Context& ctx, shared_ptr<U> value)->decltype(TypeInfo<U>::apply(ctx, value), void{}) {
				apply_ptr(ctx, value);
				TypeInfo<T>::apply(ctx, std::move(value));
			}

		public:
			static void construct(Context& ctx, Shared<T> value) {
				ctx.push(This{ });
				apply(ctx, std::move(value.object));
				ctx.pop();
			}
			static void push(Context& ctx, Shared<T> value) {
				StackAssert sa(ctx, 1);
				ctx.push(Object{ });
				assert(value.object);
				if (value.object) value.object->prototype(ctx);
				duk_set_prototype(ctx, -2);
				apply(ctx, value.object);
			}
			static shared_ptr<T> get(Context& ctx, int index) {
				ctx.get_property<void>(index, T::JSName);

				if (ctx.type(-1) == DUK_TYPE_UNDEFINED) {
					//ctx.print_dump();
					ctx.pop();
					ctx.raise(ReferenceError("invalid this binding"));
				}
				else ctx.pop();

				duk_get_prop_string(ctx, index, "\xff""\xff""js-shared-ptr");
				shared_ptr<T> value = *static_cast<shared_ptr<T>*>(duk_to_pointer(ctx, -1));
				ctx.pop();
				return value;
			}
		};

		class CallbackCaller;

		template<typename... Targs>
		class Callback {
			friend CallbackCaller;

		public:
			static constexpr std::size_t NumArgs = sizeof...(Targs);

			Callback(Context& js, int idx) : m_js((duk_context*)js) {
				// GlobalStash.callbacks[m_id] = {Callback: this, function: m_js.dup(idx)}
				init(idx);
			}
			Callback(const Callback&) = delete;
			virtual ~Callback() {
				// delete GlobalStash.callbacks[m_id]
				StackAssert sa(m_js);
				m_js.push(GlobalStash{ });
				m_js.get_property<void>(-1, "callbacks");
				if (m_js.is<Array>(-1))
					m_js.delete_property(-1, m_id);
				m_js.pop(2);
			}

			inline int id() const { return m_id; }
			inline Context& context() { return m_js; }

			virtual bool is_method() const { return false; }

			inline void operator()(Targs... args) {
				CallbackCaller::call(*this, std::forward<Targs>(args)...);
			}

		protected:
			Callback(Context& js) : m_js((duk_context*)js) {
				// GlobalStash.callbacks[m_id] = {Callback: this, function: m_js.dup(idx)}
			}

			void init(int idx) {
				StackAssert sa(m_js);
				idx = m_js.normalize_index(idx);
				if (!m_js.is<Function>(idx))
					m_js.raise(TypeError("function"));

				m_js.push(GlobalStash{ });
				m_js.get_property<void>(-1, "callbacks");
				if (!m_js.is<Array>(-1)) {
					m_js.pop();
					m_js.push(Array{ });
					m_js.put_property(-2, "callbacks");
					m_js.get_property<void>(-1, "callbacks");
				}

				int id, len = m_js.length(-1);
				for (id = 0; id < len; ++id) {
					m_js.get_property<void>(-1, id);
					bool b = m_js.is<Object>(-1);
					m_js.pop();
					if (!b) break;
				}

				m_id = id;
				push_object_js(idx);
				m_js.put_property(-2, m_id);
				m_js.pop(2);
			}

			virtual void push_object_js(int fnIdx) {
				m_js.push_object(
					Property<RawPointer<void>>{"Callback", RawPointer<void>{ this }}
				);
				m_js.dup(fnIdx);
				m_js.put_property(-2, "function");
			}
			virtual void push_function_js() {
				m_js.get_property<void>(-1, "function");
			}

		protected:
			Context m_js;
			int m_id;
		};
		template<typename... Targs>
		class CallbackMethod : public Callback<Targs...> {
		public:
			CallbackMethod(Context& js) : Callback(js) {
				init(-2);
			}

			virtual bool is_method() const { return true; }

		protected:
			virtual void push_object_js(int fnIdx) {
				m_js.push_object(
					Property<RawPointer<void>>{"Callback", RawPointer<void>{ this }}
				);
				m_js.dup(fnIdx);
				m_js.put_property(-2, "function");
				m_js.dup(fnIdx + 1);
				m_js.put_property(-2, "object");
			}
			virtual void push_function_js() {
				m_js.get_property<void>(-1, "function");
				m_js.get_property<void>(-2, "object");
			}
		};

		class CallbackCaller {
		public:
			static void call(Callback<>& cb) {
				StackAssert sa(cb.m_js, 1);
				cb.m_js.push(GlobalStash{ });
				cb.m_js.get_property<void>(-1, "callbacks");
				cb.m_js.get_property<void>(-1, cb.m_id);
				cb.push_function_js();
				if (cb.is_method()) cb.m_js.pcall_method(0);
				else cb.m_js.pcall(0);
				cb.m_js.swap(-1, -4);
				cb.m_js.pop(3);
			}
			template<typename Targ, typename... Targs>
			static void call(Callback<Targ, Targs...>& cb, Targ&& arg, Targs&&... args) {
				StackAssert sa(cb.m_js, 1);
				cb.m_js.push(GlobalStash{ });
				cb.m_js.get_property<void>(-1, "callbacks");
				cb.m_js.get_property<void>(-1, cb.m_id);
				cb.push_function_js();
				cb.m_js.push(arg, std::forward<Targs>(args)...);
				auto nargs = cb.NumArgs;
				if (cb.is_method()) cb.m_js.pcall_method(nargs);
				else cb.m_js.pcall(nargs);
				cb.m_js.swap(-1, -4);
				cb.m_js.pop(3);
			}
		};

		// Returns true if the variant map represents a JS/C++ object
		template<typename T>
		bool is_object(const VariantMap& vm) {
			if (vm.find(T::JSName) != vm.end()) {
				auto it = vm.find("\xff""\xff""js-ptr");
				return it != vm.end() && it->second.type() == typeid(VarPointer);
			}
			return false;
		}

		// Returns the C++ object associated with a variant map representing a JS object or a nullptr on invalid representation
		template<typename T>
		T* to_object(const VariantMap& vm) {
			auto it = vm.find("\xff""\xff""js-ptr");
			if (it != vm.end() && vm.find(T::JSName) != vm.end()) {
				if (it->second.type() == typeid(VarPointer)) {
					auto vp = std::any_cast<VarPointer>(it->second);
					if (vp.type == PointerType::SharedPointer)
						return (*reinterpret_cast<shared_ptr<T>*>(vp.ptr)).get();
					return reinterpret_cast<T*>(vp.ptr);
				}
			}
			return nullptr;
		}
	}
}