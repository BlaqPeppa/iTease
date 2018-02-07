#include "stdinc.h"
#include "db.h"
#include "logging.h"
#include "cpp/string.h"

using namespace iTease;

optional<Version> Database::query_version() {
	optional<Version> ret;
	Version ver;
	if (table_exists("setup")) {
		*m_db << "SELECT value FROM setup WHERE name = ?"
			<< "version"
			>> [&](string value) { ret = value; };
	}
	return ret;
}

// initial DB setup
bool DatabaseVersion<0, 0>::upgrade(Database& db) {
	db << "BEGIN;";
	try {
		db << "DROP TABLE IF EXISTS setup";
		//db << "DROP TABLE IF EXISTS users";
		db << "DROP TABLE IF EXISTS plugins";
		db << "CREATE TABLE setup ("
			"	name	TEXT PRIMARY KEY ON CONFLICT REPLACE NOT NULL,"
			"	value	TEXT"
			")";
		db << "INSERT INTO setup (name, value) VALUES (?, ?)"
			<< "upgrade_version"
			<< fmt::format("{}.{}.{}", db.version().major, db.version().minor, db.version().beta);
		db << "INSERT INTO setup (name, value) VALUES (?, ?)"
			<< "version"
			<< fmt::format("{}.{}.{}", Version::Latest.major, Version::Latest.minor, Version::Latest.beta);

		db << "CREATE TABLE IF NOT EXISTS library ("
			"	'id'	TEXT(16)		PRIMARY KEY					NOT NULL,"
			"	'name'	TEXT(64)		NOT NULL,"
			"	'type'	TEXT(8)			NOT NULL,"
			"	'path'	TEXT(300),"
			"	'sort'	INT				DEFAULT(0)"
			")";
		db << "CREATE TABLE IF NOT EXISTS users ("
			"	'id'	INTEGER			PRIMARY KEY					NOT NULL,"
			"	'name'	TEXT(64)		UNIQUE						NOT NULL,"
			"	'email' TEXT(120)		UNIQUE						NOT NULL,"
			"	'pass'	TEXT(255)		NOT NULL,"
			"	'added'	DATETIME		DEFAULT(CURRENT_TIMESTAMP)	NOT NULL,"
			"	'level'	INT				DEFAULT(1)					NOT NULL,"
			"	'xp'	INT				DEFAULT(0)					NOT NULL,"
			"	'setup'	INT				DEFAULT(0)					NOT NULL,"
			"	'dob'	DATE			NOT NULL"
			")";
		db << "COMMIT;";
		return true;
	}
	catch (const sqlite::sqlite_exception& ex) {
		db << "ROLLBACK;";
		throw(ex);
	}
	catch (...) {
		db << "ROLLBACK;";
		throw;
	}
	return false;
}
bool DatabaseVersion<0, 1>::upgrade(Database& db) {
	return true;
}
bool DatabaseVersion<0, 2>::upgrade(Database& db) {
	return true;
}
bool DatabaseVersion<0, 3>::upgrade(Database& db) {
	if (db.version() < PreviousVersion::version) {
		if (PreviousVersion::upgrade(db))
			return false;
	}
	return true;
}