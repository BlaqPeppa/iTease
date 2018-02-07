#include "stdinc.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <curl/curl.h>
#include <regex>
#include <cmath>
#include "user.h"
#include "application.h"

using namespace iTease;

string PBKDF2_HMAC_SHA_256(const string& pass, const string& salt, size_t size, int32_t iterations) {
	string output;
	output.resize(size);
	PKCS5_PBKDF2_HMAC(pass.c_str(), pass.size(), (const unsigned char*)salt.c_str(), salt.size(), iterations, EVP_sha256(), output.size(), (unsigned char*)output.data());
	std::stringstream ss;
	ss << std::hex << std::setfill('0');
	for (auto& c : output)
		ss << std::setw(2) << static_cast<int>(*reinterpret_cast<unsigned char*>(&c));
	return ss.str();
}
string PBKDF2_HMAC_SHA_512(const string& pass, const string& salt, size_t size, int32_t iterations) {
	string output;
	output.resize(size);
	PKCS5_PBKDF2_HMAC(pass.c_str(), pass.size(), (const unsigned char*)salt.c_str(), salt.size(), iterations, EVP_sha512(), output.size(), (unsigned char*)output.data());
	std::stringstream ss;
	ss << std::hex << std::setfill('0');
	for (auto& c : output)
		ss << std::setw(2) << static_cast<int>(*reinterpret_cast<unsigned char*>(&c));
	return ss.str();
}

auto Password::check(const string& packed, const string& password)->bool {
	string hash, salt;
	unsigned cost;
	if (unpack(packed, hash, salt, cost))
		return Password::hash_compare(Password::hash(password, salt, cost), hash);
	return false;
}
auto Password::pack(const string& hash, const string& salt, unsigned cost)->string {
	return "$" + std::to_string(salt.size()) + "$" + std::to_string(cost) + "$" + salt + hash;
}
auto Password::unpack(const string& packed, string& hash, string& salt, unsigned& cost)->bool {
	if (packed.size() < 7) return false;
	if (packed[0] != '$') return false;
	string str;
	int i = 1;
	while (std::isdigit(packed[i]) && i < 4) {
		str += packed[i];
		++i;
	}
	if (packed[i++] != '$') return false;
	auto saltlen = std::stoul(str);
	str = "";
	while (std::isdigit(packed[i]) && i < 12) {
		str += packed[i];
		++i;
	}
	if (packed[i++] != '$') return false;
	if ((packed.size() - i) <= saltlen) return false;
	cost = std::stoul(str);
	salt = packed.substr(i, saltlen);
	hash = packed.substr(i + saltlen);
	return true;
}
auto Password::salt(size_t size)->string {
	return rand_string(size, "0123456789abcdef");
}
auto Password::hash(const string& pass)->string {
	unsigned cost;
	auto slt = salt();
	auto ph = hash(pass, slt, &cost);
	return pack(ph, slt, cost);
}
auto Password::hash(const string& pass, const string& salt, unsigned cost)->string {
	return PBKDF2_HMAC_SHA_512(pass, salt, 28, cost);
}
auto Password::hash(const string& pass, const string& salt, unsigned* cost)->string {
	if (cost) *cost = 65536;
	return PBKDF2_HMAC_SHA_512(pass, salt, 28, 65536);
}
auto Password::hash_compare(const string& a, const string& b)->bool {
	return a == b;
}

User::User(UserData data) : m_data(data)
{ }
User::User(uint64_t id, string name, string email, string password, date::year_month_day dob) : m_data{
	id,
	name, 
	email,
	password,
	date::sys_seconds(std::chrono::duration_cast<std::chrono::seconds>(date::sys_seconds::clock::now().time_since_epoch())),
	dob
}
{ }
auto User::validate_name(const string& name)->NameValidation {
	static const std::regex pattern("[\\w\\-\\.]+");
	if (name.size() < 2) return NameValidation::TooShort;
	if (name.size() > 64) return NameValidation::TooLong;
	if (!std::regex_match(name, pattern)) return NameValidation::InvalidCharacters;
	return NameValidation::OK;
}
auto User::validate_email(const string& email)->EmailValidation {
	static const std::regex pattern("[^\\s@]+@[^\\s@]+\\.[^\\s@]+");
	if (email.empty()) return EmailValidation::Empty;
	if (email.size() > 64) return EmailValidation::TooLong;
	if (!std::regex_match(email, pattern)) return EmailValidation::Invalid;
	return EmailValidation::OK;
}
auto User::validate_password(const string& pass)->PasswordValidation {
	if (pass.size() > 100) return PasswordValidation::TooLong;
	else if (pass.size() < 5) return PasswordValidation::TooShort;
	return PasswordValidation::OK;
}
auto User::prototype(JS::Context& js)->void {
	JS::StackAssert sa(js, 1);

	js.pop();
	js.push(*this);

	// return prototype
	js.get_global<void>("\xff""\xff""module-users");
	js.get_property<void>(-1, JSName);
	js.remove(-2);
}

Users::Users(Application& app) : Module("users"), m_app(app) {
	// add event to auto-save users, called every 20 seconds
	m_onTickEventListener = m_app.add_interval_callback(20000, [this](long long& delay)->int { 
		try {
			for (auto& pr : m_users) {
				save_user(*pr.second);
			}
		}
		catch (const sqlite::sqlite_exception& ex) {
			throw ex;
		}
		return true;
	});
}
auto Users::save_user(User& user)->void {
	m_updateUserQuery << user.data().dob	// set
		<< user.data().level
		<< user.data().xp
		<< user.data().setup_stage
		<< user.data().id;					// where
	m_updateUserQuery++;
}
auto Users::get_usernames()->vector<string> {
	vector<string> names;
	m_findAllUserNamesQuery >> [&](string name) { names.emplace_back(name); };
	return names;
}
auto Users::get_user(int64_t userid)->shared_ptr<User> {
	auto it = m_users.find(userid);
	if (it != m_users.end()) return it->second;
	optional<UserData::TupleNoRefs> ud;
	m_findUserRowidQuery << userid >> ud;
	return ud ? create_user_from_db(std::make_from_tuple<UserData>(*ud)) : nullptr;
}
auto Users::get_user(const string& username)->shared_ptr<User> {
	auto it = m_userNames.find(username);
	if (it != m_userNames.end()) return it->second;

	optional<UserData::TupleNoRefs> ud;
	m_findUserNameQuery << username >> ud;
	return ud ? create_user_from_db(std::make_from_tuple<UserData>(*ud)) : nullptr;
}
auto Users::get_user_by_email(const string& email)->shared_ptr<User> {
	optional<UserData::TupleNoRefs> ud;
	m_findUserEmailQuery << email >> ud;
	return ud ? create_user_from_db(std::make_from_tuple<UserData>(*ud)) : nullptr;
}
auto Users::create_user(const string& username, const string& email, const string& password, date::year_month_day dob)->shared_ptr<User> {
	auto pass_hash = Password::hash(password);
	try {
		UserData::TupleNoRefs ud;
		m_insertUserQuery << username << email << pass_hash << dob;
		m_insertUserQuery++;
		++m_numUsers;
		m_findUserRowidQuery << m_app.db->connection()->last_insert_rowid() >> ud;
		return create_user_from_db(std::make_from_tuple<UserData>(ud));
	}
	catch (const sqlite::sqlite_exception& ex) {
		auto c = ex.get_code();
		auto ec = ex.get_extended_code();
		ITEASE_LOGERROR(ex.what());
	}
	return nullptr;
}
auto Users::create_user_from_db(const UserData& ud)->shared_ptr<User> {
	auto user = std::make_shared<User>(ud);
	auto pr = m_users.emplace(ud.id, user);
	auto pr2 = m_userNames.emplace(ud.name, user);
	if (!pr.second) throw std::bad_alloc();
	if (!pr2.second) throw std::bad_alloc();
	return std::move(user);
}
void Users::init_module(Application& app) {
	*m_app.db << "SELECT COUNT(*) FROM users" >> m_numUsers;
	m_onRequestEventListener = app.OnServerRequestInternal.listen([this](const ServerRequest& request, ServerResponse& response) {
		if (request.uri == "/signin") {
			auto unit = request.post_data.find("username");
			auto pwit = request.post_data.find("password");
			auto end = request.post_data.end();
			json jsobj;
			bool success = false;
			if (unit != end && pwit != end) {
				auto user = User::validate_email(unit.value()) == User::EmailValidation::OK ? get_user_by_email(unit.value()) : get_user(unit.value());
				if (user) {
					if (user->check_password(pwit.value())) {
						success = true;
						request.session->data = user;
					}
					else jsobj["errors"]["password"] = "Incorrect password";
				}
				else jsobj["errors"]["username"] = "Username/email not found";
			}
			else jsobj["errors"] = "Missing required fields";
			jsobj["success"] = success;
			response.content = jsobj.dump();
			response.content_type = get_content_type_by_extension("json");
			response.status = 200;
			return true;
		}
		else if (request.uri == "/signup") {
			auto unit = request.post_data.find("username");
			auto emit = request.post_data.find("email");
			auto pwit = request.post_data.find("password");
			auto pcit = request.post_data.find("confirm_password");
			auto dobit = request.post_data.find("dob");
			auto end = request.post_data.end();
			json jsobj;
			bool success = false;
			if (unit != end && emit != end && pwit != end && pcit != end && dobit != end) {
				auto name = unit.value();
				auto email = emit.value();
				auto password = pwit.value();
				auto cpassword = pcit.value();
				auto dobstr = dobit.value();
				auto nameval = User::validate_name(name);
				auto emlval = User::validate_email(email);
				auto pwval = User::validate_password(password);
				date::year_month_day dob;
				if (nameval == User::NameValidation::TooShort)
					jsobj["errors"]["username"] = "Username too short";
				else if (nameval == User::NameValidation::TooLong)
					jsobj["errors"]["username"] = "Username too long";
				else if (nameval == User::NameValidation::InvalidCharacters)
					jsobj["errors"]["username"] = "Username contains invalid characters";
				if (emlval == User::EmailValidation::Empty)
					jsobj["errors"]["email"] = "Email required";
				else if (emlval == User::EmailValidation::TooLong)
					jsobj["errors"]["email"] = "Email too long";
				else if (emlval == User::EmailValidation::Invalid)
					jsobj["errors"]["email"] = "Invalid email";
				if (pwval == User::PasswordValidation::TooShort)
					jsobj["errors"]["password"] = password.empty() ? "Password required" : "Password too short";
				else if (pwval == User::PasswordValidation::TooLong)
					jsobj["errors"]["password"] = "Password too long";
				if (password != cpassword)
					jsobj["errors"]["confirm_password"] = "Passwords do not match";
				auto dobss = std::istringstream(dobstr);
				dobss >> date::parse("%F", dob);
				if (!dobss) jsobj["errors"]["dob"] = "Invalid date";
				else {
					auto today = date::year_month_day{floor<date::days>(std::chrono::system_clock::now())};
					auto age = today.year() - dob.year();
					if (today.month() < dob.month() || (today.month() == dob.month() && today.day() < today.day())) --age;
					// gotta have some fun
					if (age.count() < 0) jsobj["errors"]["dob"] = "Sorry, this software does not yet support scenarios involving time travel";
					else if (age.count() < 18) jsobj["errors"]["dob"] = "Sorry, but you should be 18 or older to use this software";
					else if (age.count() > 120) jsobj["errors"]["dob"] = "Really? You don't look a day over 100!";
				}
				if (jsobj["errors"].find("username") == jsobj["errors"].end()) {
					if (get_user(name))
						jsobj["errors"]["username"] = "Username already registered";
				}
				if (jsobj["errors"].find("email") == jsobj["errors"].end()) {
					if (get_user_by_email(name))
						jsobj["errors"]["email"] = "Email already registered";
				}
				if (!jsobj["errors"].size()) {
					if (auto user = create_user(name, email, password, dob)) {
						request.session->data = user;
						success = true;
					}
					else jsobj["errors"] = "Failed to create user";
				}
			}
			else jsobj["errors"] = "Missing required fields";
			jsobj["success"] = success;
			response.content = jsobj.dump();
			response.content_type = get_content_type_by_extension("json");
			response.status = 200;
			return true;
		}
		else if (request.uri == "/_password.json") {
			auto get_it = request.params.find("test_generate");
			if (get_it != request.params.end()) {
				auto start = std::chrono::high_resolution_clock::now();
				std::chrono::high_resolution_clock::duration elapsed;
				string hash;

				unsigned cost = 8;
				auto salt = Password::salt();
				do {
					cost <<= 1;
					start = std::chrono::high_resolution_clock::now();
					hash = Password::hash("password", salt, cost);
					elapsed = std::chrono::high_resolution_clock::now() - start;
					std::cout << cost << ": " << std::to_string((double)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0) << "s" << std::endl;
				} while (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 500);
				json js;
				js["cost"] = cost;
				js["time"] = std::to_string((double)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0) + "s";
				js["hash"] = Password::pack(hash, salt, cost);
				response.content = js.dump();
				response.status = 200;
				response.content_type = get_content_type_by_extension("json");
				return true;
			}
			else {
				auto salt = Password::salt();
				auto jsobj = json();
				for (auto& pr : request.post_data) {
					unsigned cost = 0;
					jsobj[pr.first] = Password::hash(pr.second, salt, &cost);
				}

				response.content = jsobj.dump();
				response.status = 200;
				response.content_type = get_content_type_by_extension("json");
				return true;
			}
			return true;
		}
		return false;
	});

	m_findAllUserNamesQuery = app.db->prepare("SELECT name FROM users");
	m_findUserRowidQuery = app.db->prepare("SELECT id, name, email, pass, added, dob, level, xp, setup FROM users WHERE id = ? LIMIT 1");
	m_findUserNameQuery = app.db->prepare("SELECT id, name, email, pass, added, dob, level, xp, setup FROM users WHERE name = ? LIMIT 1");
	m_findUserEmailQuery = app.db->prepare("SELECT id, name, email, pass, added, dob, level, xp, setup FROM users WHERE email = ? LIMIT 1");
	m_insertUserQuery = app.db->prepare("INSERT INTO users (name, email, pass, dob) VALUES (?, ?, ?, ?)");
	m_updateUserQuery = app.db->prepare("UPDATE users SET dob = ?, level = ?, xp = ?, setup = ? WHERE id = ?");
	m_updateUserNameQuery = app.db->prepare("UPDATE users SET name = ? WHERE id = ?");
	m_updateUserEmailQuery = app.db->prepare("UPDATE users SET email = ? WHERE id = ?");
	m_updateUserPasswordQuery = app.db->prepare("UPDATE users SET pass = ? WHERE id = ?");
}
void Users::init_js(JS::Context& js) {
	using namespace std::placeholders;
	js.push_object(
		JS::Property<JS::Object>{User::JSName, js.push_object(
			JS::Property<bool>{User::JSName, true}
		)},
		JS::Property<JS::Function>{"getUser", JS::Function{std::bind(&Users::get_user_js, this, _1), 1}},
		JS::Property<JS::Function>{"getUserByEmail", JS::Function{std::bind(&Users::get_user_by_email_js, this, _1), 1}},
		JS::Property<JS::Function>{"getUsernames", JS::Function{std::bind(&Users::get_usernames_js, this, _1), 0}},
		JS::Property<JS::Function>{"getNumUsers", JS::Function{std::bind(&Users::get_num_users_js, this, _1), 0}}
	);
	js.dup();
	js.put_global("\xff""\xff""module-users");
}
int Users::get_user_js(JS::Context& js) {
	shared_ptr<User> user;
	if (js.is<int>(0)) {
		auto id = js.get<int>(0);
		user = get_user(id);
	}
	else if (js.is<string>(0)) {
		auto name = js.get<string>(0);
		user = get_user(name);
	}
	else js.raise(JS::ParameterError("expected integer or string (arg: 0)"));
	if (user) js.push(JS::Shared<User>{user});
	else js.push(JS::Undefined{});
	return 1;
}
int Users::get_usernames_js(JS::Context& js) {
	js.push(get_usernames());
	return 1;
}
int Users::get_user_by_email_js(JS::Context& js) {
	shared_ptr<User> user;
	auto email = js.require<string>(0);
	user = get_user_by_email(email);
	if (user) js.push(JS::Shared<User>{user});
	else js.push(JS::Undefined{ });
	return 1;
}
int Users::get_num_users_js(JS::Context& js) {
	js.push(m_numUsers);
	return 1;
}