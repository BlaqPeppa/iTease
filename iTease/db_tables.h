#pragma once
#include "db.h"

namespace iTease {
	/*struct DBColumn {
	public:
		DBColumn() = default;
		DBColumn(const Database::Record& record) {
			if (record.size() < 6)
				throw(std::runtime_error("record must have 6 columns to represent DBColumn"));
			id = std::any_cast<int>(record[0]);
			name = std::any_cast<string>(record[1]);
			type = column_type_by_name(std::any_cast<string>(record[2]));
			not_null = std::any_cast<int>(record[3]) != 0;
			if (record[4].has_value())
				default_value = std::any_cast<string>(record[4]);
			primary_key = std::any_cast<int>(record[5]) != 0;
		}

		auto column_type_by_name(const string& name)->DBColumnType {
			if (name == "TEXT")
				return DBColumnType::Text;
			if (name == "INTEGER")
				return DBColumnType::Integer;
			if (name == "REAL")
				return DBColumnType::Real;
			if (name == "BLOB")
				return DBColumnType::Blob;
			return DBColumnType::Null;
		}

		unsigned id;
		string name;
		DBColumnType type;
		string default_value;
		bool not_null = false;
		bool primary_key = false;
	};

	class DBTable {
	public:
		DBTable(Database& db, string name) : db(db), name(name) {
			auto info = query_column_info();
			if (!info.rows.empty()) {
				exists = true;
				columns = std::vector<DBColumn>{info.rows.begin(), info.rows.end()};
			}
		}

		inline auto& get_name() const { return name; }
		inline auto& get_columns() const { return columns; }

		auto set_column_name(unsigned id, const string& name) {
			if (columns.size() > id) {
				columns[id].name = name;
				return true;
			}
			return false;
		}

	protected:
		auto query_column_info()->DBResult {
			return db.execute("PRAGMA table_info(" + name + ")");
		}

	protected:
		bool exists = false;
		string name;
		Database& db;
		vector<DBColumn> columns;
	};

	class SetupDBTable : public DBTable {
	public:
		SetupDBTable(Database& db) : DBTable(db, "setup") {
			db.select("");
		}

	private:
	};*/
}