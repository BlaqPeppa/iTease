#pragma once
#include "common.h"
#include "module.h"
#include "db.h"
#include "event.h"
#include "server.h"
#include "application.h"

namespace iTease {
	using std::chrono::system_clock;
	using std::chrono::time_point;

	class Password {
	public:
		static auto salt(size_t size = 16)->string;
		static auto hash(const string& pass)->string;
		static auto hash(const string& pass, const string& salt, unsigned cost)->string;
		static auto hash(const string& pass, const string& salt, unsigned* cost = nullptr)->string;
		static auto hash_compare(const string& hash1, const string& hash2)->bool;
		static auto check(const string& packed, const string& pass)->bool;
		static auto pack(const string& hash, const string& salt, unsigned cost)->string;
		static auto unpack(const string& packed, string& hash, string& salt, unsigned& cost)->bool;
	};

	struct UserData {
		uint64_t id;
		string name;
		string email;
		string password;
		date::sys_seconds registered;
		date::year_month_day dob;
		int level = 1;
		int xp = 0;
		int setup_stage = 0;

		using Tuple = decltype(std::tie(id, name, email, password, registered, dob, level, xp, setup_stage));
		using TupleNoRefs = std::tuple<uint64_t, string, string, string, date::sys_seconds, date::year_month_day, int, int, int>;

		UserData(uint64_t id, string name, string email, string password, date::sys_seconds registered, date::year_month_day dob, int level = 1, int xp = 0, int setup_stage = 0) :
			id(id), name(name), email(email), password(password), registered(registered), dob(dob), level(level), xp(xp), setup_stage(setup_stage)
		{ }
		auto as_tuple() {
			return std::tie(id, name, email, password, registered, dob, level, xp, setup_stage);
		}
		auto as_tuple() const {
			return std::tie(id, name, email, password, registered, dob, level, xp, setup_stage);
		}
	};

	class User {
	public:
		enum class NameValidation {
			OK,
			TooShort,
			TooLong,
			InvalidCharacters
		};
		enum class EmailValidation {
			OK,
			Empty,
			TooLong,
			Invalid
		};
		enum class PasswordValidation {
			OK,
			TooShort,
			TooLong
		};

		struct CreateAccountError : public std::runtime_error {
			CreateAccountError(NameValidation nv, EmailValidation ev, PasswordValidation pv) : std::runtime_error("invalid user creation arguments"), name_validation(nv), email_validation(ev), password_validation(pv)
			{ }

			NameValidation name_validation;
			EmailValidation email_validation;
			PasswordValidation password_validation;
		};

	public:
		User(uint64_t id, string name, string email, string password, date::year_month_day dob);
		User(UserData data);

		auto& data() const { return m_data; }
		auto& name() const { return m_data.name; }
		auto& email() const { return m_data.email; }
		auto& register_date() const { return m_data.registered; }
		auto& dob() const { return m_data.dob; }
		auto xp() { return m_data.xp; }

		auto check_password(const string& pass) const->bool { return Password::check(m_data.password, pass); }

		static auto validate_name(const string&)->NameValidation;
		static auto validate_email(const string&)->EmailValidation;
		static auto validate_password(const string&)->PasswordValidation;

		// JS
		static constexpr const char* JSName = "\xff""\xff""User";

		auto prototype(JS::Context& js)->void;

	private:
		UserData m_data;
	};

	class Users : public Module {
	public:
		Users(Application&);
		virtual ~Users() { }

		// creates/registers a new user andd inserts into the DB
		auto create_user(const string& name, const string& email, const string& password, date::year_month_day dob)->shared_ptr<User>;
		// creates a user with data loaded from DB, does not insert into DB
		auto create_user_from_db(const UserData&)->shared_ptr<User>;
		// finds a user with the specified ID from the DB/cache
		auto get_user(int64_t userid)->shared_ptr<User>;
		// finds a user with the specified name from the DB/cache
		auto get_user(const string& name)->shared_ptr<User>;
		// finds a user with the specified email from the DB/cache
		auto get_user_by_email(const string& email)->shared_ptr<User>;
		// save user in DB and update cache
		auto save_user(User&)->void;
		// get a list of all usernames from the DB
		auto get_usernames()->vector<string>;

		virtual void init_module(Application&) override;
		virtual void init_js(JS::Context&) override;

	private:
		int get_user_js(JS::Context&);
		int get_user_by_email_js(JS::Context&);
		int get_usernames_js(JS::Context&);
		int get_num_users_js(JS::Context&);

	private:
		Application& m_app;
		Server::OnRequestEvent::Listener m_onRequestEventListener;
		map<int64_t, shared_ptr<User>> m_users;
		map<string, shared_ptr<User>> m_userNames;
		int m_numUsers = 0;
		Application::OnTickEvent::Listener m_onTickEventListener;
		vector<sqlite::database_binder> m_queries;
		PreparedQuery m_findAllUserNamesQuery;
		PreparedQuery m_findUserRowidQuery;
		PreparedQuery m_findUserNameQuery;
		PreparedQuery m_findUserEmailQuery;
		PreparedQuery m_insertUserQuery;
		PreparedQuery m_updateUserQuery;
		PreparedQuery m_updateUserNameQuery;
		PreparedQuery m_updateUserEmailQuery;
		PreparedQuery m_updateUserPasswordQuery;
	};

	class UserJS {
	public:


	private:
		User& m_user;
		Users& m_users;
	};

	namespace JS {
		template <>
		class TypeInfo<User> {
		public:
			static void apply(Context& js, User& user) {
				// name
				js.put_property(-1, Property<string>{"name", JS::Function{[](JS::Context& js)->duk_ret_t {
					auto user = js.self<JS::Shared<User>>();
					js.push(user->name());
					return 1;
				}, 0}, JS::Function{}});
				// email
				js.put_property(-1, Property<string>{"email", JS::Function{[](JS::Context& js)->duk_ret_t {
					auto user = js.self<JS::Shared<User>>();
					js.push(user->email());
					return 1;
				}, 0}, JS::Function{ }});
				// xp
				js.put_property(-1, Property<int>{"xp", JS::Function{[](JS::Context& js)->duk_ret_t {
					auto user = js.self<JS::Shared<User>>();
					js.push<int>(user->data().xp);
					return 1;
				}, 0}, JS::Function{ }});
				// level
				js.put_property(-1, Property<int>{"level", JS::Function{[](JS::Context& js)->duk_ret_t {
					auto user = js.self<JS::Shared<User>>();
					js.push(user->data().level);
					return 1;
				}, 0}, JS::Function{ }});
			}
			static void push(Context& js, User& user) {
				StackAssert sa(js, 1);
				js.push_object();
				apply(js, std::move(user));
			}
			static void construct(Context& js, User& user) {
				js.push_this_object();
				apply(js, std::move(user));
				js.pop();
			}
		};
	}
}