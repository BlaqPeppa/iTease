#include "stdinc.h"
#include "js.h"
#include <duktape/duktape.h>

namespace iTease {
	/*template<>
	JS::VarPointer Variant::Convert() const {
		return GetType() == VariantType::JSPointer ? GetVal<JS::VarPointer>() : JS::VarPointer();
	}*/

	namespace JS {
		namespace {
			static duk_int_t duk__eval_module_source(duk_context *ctx, void *udata);
			static void duk__push_module_object(duk_context *ctx, const char *id, duk_bool_t main);

			static duk_bool_t duk__get_cached_module(duk_context *ctx, const char *id) {
				duk_push_global_stash(ctx);
				duk_get_prop_string(ctx, -1, "\xff" "requireCache");
				if (duk_get_prop_string(ctx, -1, id)) {
					duk_remove(ctx, -2);
					duk_remove(ctx, -2);
					return 1;
				}
				duk_pop_3(ctx);
				return 0;
			}

			/* Place a `module` object on the top of the value stack into the require cache
			* based on its `.id` property.  As a convenience to the caller, leave the
			* object on top of the value stack afterwards.
			*/
			static void duk__put_cached_module(duk_context *ctx) {
				// [ ... module ]
				duk_push_global_stash(ctx);
				duk_get_prop_string(ctx, -1, "\xff" "requireCache");
				duk_dup(ctx, -3);

				// [ ... module stash req_cache module ]
				duk_get_prop_string(ctx, -1, "id");
				duk_dup(ctx, -2);
				duk_put_prop(ctx, -4);
				duk_pop_3(ctx);  // [ ... module ]
			}
			static void duk__del_cached_module(duk_context *ctx, const char *id) {
				duk_push_global_stash(ctx);
				duk_get_prop_string(ctx, -1, "\xff" "requireCache");
				duk_del_prop_string(ctx, -1, id);
				duk_pop_2(ctx);
			}
			static duk_ret_t duk__handle_require(duk_context *ctx) {
				/*
				*  Value stack handling here is a bit sloppy but should be correct.
				*  Call handling will clean up any extra garbage for us.
				*/
				const char *id;
				const char *parent_id;
				duk_idx_t module_idx;
				duk_idx_t stash_idx;
				duk_int_t ret;

				duk_push_global_stash(ctx);
				stash_idx = duk_normalize_index(ctx, -1);

				duk_push_current_function(ctx);
				duk_get_prop_string(ctx, -1, "\xff" "moduleId");
				parent_id = duk_require_string(ctx, -1);
				parent_id;  // not used directly; suppress warning

							// [ id stash require parent_id ]
				id = duk_require_string(ctx, 0);
				duk_get_prop_string(ctx, stash_idx, "\xff" "modResolve");
				duk_dup(ctx, 0);   // module ID
				duk_dup(ctx, -3);  // parent ID
				duk_call(ctx, 2);

				// [ ... stash ... resolved_id ]
				id = duk_require_string(ctx, -1);

				if (!duk__get_cached_module(ctx, id)) {
					duk__push_module_object(ctx, id, 0 /*main*/);
					duk__put_cached_module(ctx);  /* module remains on stack */

												  /*
												  *  From here on out, we have to be careful not to throw.  If it can't be
												  *  avoided, the error must be caught and the module removed from the
												  *  require cache before rethrowing.  This allows the application to
												  *  reattempt loading the module.
												  */

					module_idx = duk_normalize_index(ctx, -1);

					// [ ... stash ... resolved_id module ]
					duk_get_prop_string(ctx, stash_idx, "\xff" "modLoad");
					duk_dup(ctx, -3);  // resolved ID
					duk_get_prop_string(ctx, module_idx, "exports");
					duk_dup(ctx, module_idx);
					ret = duk_pcall(ctx, 3);
					if (ret != DUK_EXEC_SUCCESS) {
						duk__del_cached_module(ctx, id);
						duk_throw(ctx);  // rethrow
					}

					if (duk_is_string(ctx, -1)) {
						duk_int_t ret;
						// [ ... module source ]
						ret = duk_safe_call(ctx, duk__eval_module_source, NULL, 2, 1);
						if (ret != DUK_EXEC_SUCCESS) {
							duk__del_cached_module(ctx, id);
							duk_throw(ctx);  // rethrow
						}
					}
					else if (duk_is_undefined(ctx, -1)) {
						duk_pop(ctx);
					}
					else {
						duk__del_cached_module(ctx, id);
						duk_error(ctx, DUK_ERR_TYPE_ERROR, "invalid module load callback return value");
					}
				}

				// fall through
				// [ ... module ]
				duk_get_prop_string(ctx, -1, "exports");
				return 1;
			}
			static void duk__push_require_function(duk_context *ctx, const char *id) {
				duk_push_c_function(ctx, duk__handle_require, 1);
				duk_push_string(ctx, "name");
				duk_push_string(ctx, "require");
				duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
				duk_push_string(ctx, id);
				duk_put_prop_string(ctx, -2, "\xff" "moduleId");

				// require.cache
				duk_push_global_stash(ctx);
				duk_get_prop_string(ctx, -1, "\xff" "requireCache");
				duk_put_prop_string(ctx, -3, "cache");
				duk_pop(ctx);

				// require.main
				duk_push_global_stash(ctx);
				duk_get_prop_string(ctx, -1, "\xff" "mainModule");
				duk_put_prop_string(ctx, -3, "main");
				duk_pop(ctx);
			}
			static void duk__push_module_object(duk_context *ctx, const char *id, duk_bool_t main) {
				duk_push_object(ctx);

				// Set this as the main module, if requested
				if (main) {
					duk_push_global_stash(ctx);
					duk_dup(ctx, -2);
					duk_put_prop_string(ctx, -2, "\xff" "mainModule");
					duk_pop(ctx);
				}

				/* Node.js uses the canonicalized filename of a module for both module.id
				* and module.filename.  We have no concept of a file system here, so just
				* use the module ID for both values.
				*/
				duk_push_string(ctx, id);
				duk_dup(ctx, -1);
				duk_put_prop_string(ctx, -3, "filename");
				duk_put_prop_string(ctx, -2, "id");

				// module.exports = {}
				duk_push_object(ctx);
				duk_put_prop_string(ctx, -2, "exports");

				// module.loaded = false
				duk_push_false(ctx);
				duk_put_prop_string(ctx, -2, "loaded");

				// module.require
				duk__push_require_function(ctx, id);
				duk_put_prop_string(ctx, -2, "require");
			}
			static duk_int_t duk__eval_module_source(duk_context *ctx, void *udata) {
				(void)udata;

				/* Wrap the module code in a function expression.  This is the simplest
				* way to implement CommonJS closure semantics and matches the behavior of
				* e.g. Node.js.
				*/
				duk_push_string(ctx, "(function(exports,require,module,__filename,__dirname){");
				duk_dup(ctx, -2);  // source
				duk_push_string(ctx, "})");
				duk_concat(ctx, 3);

				// [ ... module source func_src ]
				duk_get_prop_string(ctx, -3, "filename");
				duk_compile(ctx, DUK_COMPILE_EVAL);
				duk_call(ctx, 0);

				// [ ... module source func ]
				// Set name for the wrapper function
				duk_push_string(ctx, "name");
				duk_push_string(ctx, "main");
				duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);

				// call the function wrapper
				duk_get_prop_string(ctx, -3, "exports");   // exports
				duk_get_prop_string(ctx, -4, "require");   // require
				duk_dup(ctx, -5);                                 // module
				duk_get_prop_string(ctx, -6, "filename");  // __filename
				duk_push_undefined(ctx);                          // __dirname
				duk_call(ctx, 5);

				// [ ... module source result(ignore) ]
				// module.loaded = true
				duk_push_true(ctx);
				duk_put_prop_string(ctx, -4, "loaded");
				// [ ... module source retval ]
				duk_pop_2(ctx);
				// [ ... module ]
				return 1;
			}

			/* Load a module as the 'main' module. */
			duk_ret_t duk_module_node_peval_main(duk_context *ctx, const char *path) {
				//  Stack: [ ... source ]
				duk__push_module_object(ctx, path, 1 /*main*/);
				// [ ... source module ]
				duk_dup(ctx, 0);
				// [ ... source module source ]
				return duk_safe_call(ctx, duk__eval_module_source, NULL, 2, 1);
			}
		}

		void Error::create(Context& ctx) const noexcept {
			ctx.get_global<void>(m_name);
			ctx.push(m_message);
			ctx.create(1);
			ctx.push(m_name);
			ctx.put_property(-2, "name");
		}

		ErrorException::ErrorException(Context& ctx, int code, bool pop) {
			if (code != 0) {
				auto map = ctx.get<Object>(-1);
				*this /*<< ctx.get_property<string>(-1, "name") << "\n"*/
					//<< ctx.get_property<string>(-1, "message") << "\n"
					<< ctx.get_property<string>(-1, "stack") << "\n";
				//ctx.pop();
				if (pop) ctx.pop();
			}
		}

		void Context::init_module_loader() {
			duk_module_node_init(*this);
		}
		/*int Context::RegisterCallback(int idx) {
			StackAssert sa(*this);
			idx = normalize_index(idx);
			Push(GlobalStash{ });
			GetProperty<void>(-1, "callbacks");
			if (!is<Array>(-1)) {
				pop();
				Push(Array{ });
				PutProperty(-2, "callbacks");
				GetProperty<void>(-1, "callbacks");
			}
			Dup(idx);

			if (!is<Function>(-1)) pop();
			else PutProperty(-2, m_callbackIdx);
			pop(2);
			return m_callbackIdx++;
		}
		bool Context::InvokeCallback(int i, int nargs) {
			StackAssert sa(*this);
			Push(GlobalStash{ }); // 1
			GetProperty<void>(-1, "callbacks"); // 2
			if (is<Array>(-1)) {
				GetProperty<void>(-1, i);
				if (is<Function>(-1)) {
					Insert(-(3 + nargs));
					pop(2);
					if (is<Function>(-(nargs + 1))) {
						PCall(nargs);
						return true;
					}
				}
			}
			pop();
			return false;
		}
		bool Context::InvokeCallbackMethod(int i, int nargs) {
			StackAssert sa(*this);
			Push(GlobalStash{ }); // 1
			GetProperty<void>(-1, "callbacks"); // 2
			if (is<Array>(-1)) {
				GetProperty<void>(-1, i);
				if (is<Function>(-1)) {
					Insert(-(3 + nargs));
					pop(2);
					if (is<Function>(-(nargs + 1))) {
						PCallMethod(nargs);
						return true;
					}
				}
			}
			pop();
			return false;
		}*/

		void Context::copy_properties(int srcIdx, int destIdx, int flags) noexcept {
			StackAssert sa(*this);
			destIdx = normalize_index(destIdx);
			enumerate(srcIdx, flags, true, [this, destIdx](const string& key) {
				dup(-1);
				put_property(destIdx, key);
			});
		}

		void TypeInfo<Variant>::push(Context& ctx, const Variant& var) {
			if (var.type() == typeid(bool)) TypeInfo<bool>::push(ctx, std::any_cast<bool>(var));
			else if (var.type() == typeid(int)) TypeInfo<int>::push(ctx, std::any_cast<int>(var));
			else if (var.type() == typeid(double)) TypeInfo<double>::push(ctx, std::any_cast<double>(var));
			else if (var.type() == typeid(string)) TypeInfo<string>::push(ctx, std::any_cast<string>(var));
			else if (var.type() == typeid(vector<Variant>)) TypeInfo<vector<Variant>>::push(ctx, std::any_cast<vector<Variant>>(var));
			else if (var.type() == typeid(map<string, Variant>)) TypeInfo<map<string, Variant>>::push(ctx, std::any_cast<map<string, Variant>>(var));
			else TypeInfo<JS::Undefined>::push(ctx, JS::Undefined{});
		}
		VariantVector TypeInfo<Array>::get(Context& ctx, int idx) {
			VariantVector obj;
			duk_enum(ctx, idx, 0);
			while (duk_next(ctx, -1, 1)) {
				if (ctx.is<bool>(-1)) obj.emplace_back(ctx.get<bool>(-1));
				else if (ctx.is<int>(-1)) obj.emplace_back(ctx.get<int>(-1));
				else if (ctx.is<double>(-1)) obj.emplace_back(ctx.get<double>(-1));
				else if (ctx.is<string>(-1)) obj.emplace_back(ctx.get<string>(-1));
				else if (ctx.is<Array>(-1)) obj.emplace_back(ctx.get<Array>(-1));
				else if (ctx.is<Function>(-1)) obj.emplace_back<Function>(Function{});
				else if (ctx.is<Object>(-1)) obj.emplace_back(ctx.get<Object>(-1));
				ctx.pop(2);
			}
			ctx.pop();
			return obj;
		}
		VariantMap TypeInfo<Object>::get(Context& ctx, int idx) {
			VariantMap obj;
			duk_enum(ctx, idx, DUK_ENUM_INCLUDE_HIDDEN | DUK_ENUM_INCLUDE_SYMBOLS);
			while (duk_next(ctx, -1, 1)) {
				auto key = ctx.get<string>(-2);
				if (ctx.is<Undefined>(-1)) obj[key].reset();
				else if (ctx.is<bool>(-1)) obj[key] = ctx.get<bool>(-1);
				else if (ctx.is<int>(-1)) obj[key] = ctx.get<int>(-1);
				else if (ctx.is<double>(-1)) obj[key] = ctx.get<double>(-1);
				else if (ctx.is<string>(-1)) obj[key] = ctx.get<string>(-1);
				else if (ctx.is<Array>(-1)) obj[key].emplace<VariantVector>(TypeInfo<Array>::get(ctx, -1));
				else if (ctx.is<Function>(-1)) obj[key].emplace<Function>();
				else if (ctx.is<Object>(-1)) obj[key].emplace<VariantMap>(ctx.get<Object>(-1));
				else if (ctx.is<RawPointer<void>>(-1)) {
					Variant v;
					if (key == "\xff""\xff""js-shared-ptr")
						obj["\xff""\xff""js-ptr"] = VarPointer{PointerType::SharedPointer, ctx.get<RawPointer<void>>(-1)};
					else if (key == "\xff""\xff""js-ptr")
						obj["\xff""\xff""js-ptr"] = VarPointer{PointerType::Pointer, ctx.get<RawPointer<void>>(-1)};
					else
						obj["\xff""\xff""js-ptr"] = VarPointer{PointerType::RawPointer, ctx.get<RawPointer<void>>(-1)};
				}
				ctx.pop(2);
			}
			ctx.pop();
			return obj;
		}
	}
}