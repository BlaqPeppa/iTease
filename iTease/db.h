#pragma once
#include <iterator>
#include <string_view>
#include "common.h"
#include <sqlite_modern_cpp/sqlite_modern_cpp.h>
#include "iTease.h"
#include "event.h"
#include "logging.h"

namespace sqlite {
	inline database_binder& operator<<(database_binder& db, const date::year_month_day& val) {
		db << fmt::format("{}-{}-{}", val.year(), val.month(), val.day());
		return db;
	}
	inline void get_col_from_db(database_binder& db, int inx, date::year_month_day& s) {
		std::string str;
		get_col_from_db(db, inx, str);
		if (!str.empty()) {
			std::istringstream ss(str);
			ss >> date::parse("%F %T", s);
		}
		else s = date::year_month_day();
	}
	inline void get_col_from_db(database_binder& db, int inx, date::sys_seconds& s) {
		std::string str;
		get_col_from_db(db, inx, str);
		if (!str.empty()) {
			std::istringstream ss(str);
			ss >> date::parse("%F %T", s);
		}
		else s = date::sys_seconds();
	}
	inline void get_val_from_db(sqlite3_value *value, date::sys_seconds& s) {
		std::string str;
		get_val_from_db(value, str);
		if (!str.empty()) {
			std::istringstream ss(str);
			ss >> date::parse("%F %T", s);
		}
		else s = date::sys_seconds();
	}
}

namespace iTease {
	class Application;
	class Database;

	using Connection = sqlite::database;

	/**
	 * DatabaseVersion struct handles upgrades to the database between program versions
	 * Instantiation of the 'upgrade' function is required for the current version (for Database::upgrade), plus any versions required to upgrade to before upgrading to the current
	 * Specialization of the struct may be necessary with replacing 'PreviousVersion' to skip versions or allow direct upgrades between non-sequential versions
	 * Specialization will also be necessary for every major version - database changes should not occur for beta/alpha versions
	**/
	template<int TVersionMajor = ITEASE_VERSION_MAJOR, int TVersionMinor = ITEASE_VERSION_MINOR>
	struct DatabaseVersion {
		// The version that came before. We 'specialize' this template 'generalization' under the presumption we're upgrading from a lower minor version.
		// Upgrades between major versions will require a specialization.
		using PreviousVersion = DatabaseVersion<TVersionMajor, TVersionMinor - 1>;
		// Used to determine the relevant Version of each DatabaseVersion at runtime
		static constexpr Version version = {ITEASE_VERSION_MAJOR, ITEASE_VERSION_MINOR};
		// Upgrade database to this DatabaseVersion
		static bool upgrade(Database&);
	};

	/*class DatabaseBinder {
	public:
		DatabaseBinder(sqlite::database_binder&& binder) : m_db(std::move(binder))
		{ }
		DatabaseBinder(const DatabaseBinder&) = delete;
		DatabaseBinder(DatabaseBinder&& v) : DatabaseBinder(std::move(v.m_db))
		{ }

		inline auto& operator<<(const date::year_month_day& arg) {
			*this << fmt::format("{}/{}/{}", arg.year(), arg.month(), arg.day());
			return *this;
		}
		inline auto& operator>>(date::year_month_day& arg) {
			std::stringstream ss;
			m_db >> [&ss](const string& v) { ss << v; };
			ss >> date::parse("%F", arg);
			return *this;
		}
		inline auto& operator>>(date::sys_seconds arg) {
			std::stringstream ss;
			m_db >> [&ss](const string& v) { ss << v; };
			ss >> date::parse("%F %T", arg);
			return *this;
		}
		template<typename Targ>
		inline auto& operator>>(Targ&& arg) {
			m_db >> arg;
			return *this;
		}
		template<typename Targ>
		inline auto& operator<<(Targ&& arg) {
			m_db << std::forward<Targ>(arg);
			return *this;
		}

	private:
		sqlite::database_binder m_db;
	};*/

	/**
	 * Prepared query - invalid when Database is destroyed
	**/
	class PreparedQuery {
		friend class Database;

	public:
		PreparedQuery() = default;
		PreparedQuery(const PreparedQuery& v) : m_vec(v.m_vec), m_idx(v.m_idx)
		{ }

		operator bool() const { return m_vec != nullptr; }
		auto& operator++(int) const {
			get()++;
			return *this;
		}
		auto& operator*() const { return get(); }
		auto operator->() const { return &get(); }
		template<typename T>
		auto& operator<<(T&& v) {
			get() << v;
			return *this;
		}

		template<typename T>
		typename std::enable_if<sqlite::database_binder::is_sqlite_value<T>::value, void>::type operator>>(T& value) {
			get() >> value;
			return *this;
		}
		template<typename... Types>
		void operator>>(std::tuple<Types...>& values) {
			get() >> std::forward<std::tuple<Types...>>(values);
		}
		template<typename... T>
		void operator>>(optional<std::tuple<T...>>& v) {
			get() >> v;
		}
		template<typename T>
		void operator>>(T&& v) {
			get() >> v;
		}

		sqlite::database_binder& get() const { return (*m_vec)[m_idx]; }

	private:
		PreparedQuery(vector<sqlite::database_binder>& vec, vector<sqlite::database_binder>::size_type idx) : m_vec(&vec), m_idx(idx) {
			get().used(true);
		}

	private:
		vector<sqlite::database_binder>* m_vec = nullptr;
		vector<sqlite::database_binder>::size_type m_idx;
	};

	/**
	 * Database class handles operations on the native DB connection specific to this application
	**/
	class Database {
	public:
		Database(shared_ptr<Connection> db) : m_db(db) {
			m_queries.reserve(20);
			if (auto opt = query_version())
				m_version = *opt;
		}
		Database(string db_name, const sqlite::sqlite_config& config = {}) : Database(std::make_shared<Connection>(db_name, config)) {
			ITEASE_LOGDEBUG("Opening database '" << db_name << "'");
		}
		Database(const Database&) = delete;

		// Returns true if the table exists
		auto table_exists(const string& name) {
			int count = 0;
			*m_db << "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?"
				<< name
				>> count;
			return count > 0;
		}

		// Returns ID of prepared query
		auto prepare(const string& sql)->PreparedQuery {
			m_queries.emplace_back(*m_db << sql);
			return PreparedQuery{m_queries, m_queries.size() - 1};
		}
		// Returns connection handle
		auto connection() { return m_db; }
		// Returns Version of DB structure
		auto& version() const { return m_version; }
		// Returns true if DB structure is up to date with app version
		auto is_upgraded() { return m_version >= Version::Latest; }
		// Upgrades DB structure to the requested version, or the current version if none is specified, returns true if the database is or ends up being upgraded
		// Version 0.0.0 upgrade is to be treated as the initial setup and is always performed
		template<typename T = DatabaseVersion<>>
		auto upgrade() {
			if (!m_version || m_version < T::version) {
	 			if (T::upgrade(*this)) {
					m_version = T::version;
					return true;
				}
			}
			return false;
		}
		inline operator Connection&() { return *m_db; }
		inline operator const Connection&() const { return *m_db; }
		
		/*inline auto& operator<<(const date::year_month_day& arg) {
			return *this << fmt::format("{}/{}/{}", arg.year(), arg.month(), arg.day());
		}*/

		template<typename Targ>
		inline auto operator<<(Targ&& arg) {
			return *m_db << std::forward<Targ>(arg);
		}

	private:
		optional<Version> query_version();

	private:
		Version m_version;
		shared_ptr<Connection> m_db;
		vector<sqlite::database_binder> m_queries;
	};
}