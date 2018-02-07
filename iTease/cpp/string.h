#pragma once
#include <string>

namespace iTease {
	using std::string;
	using std::wstring;

	wstring to_wstring(const char*);
	wstring to_wstring(const string&);
	string to_string(const wchar_t*);
	string to_string(const wstring&);

	string_view ltrim(string_view subj, const string& str = " \t\n\r");
	string_view rtrim(string_view subj, const string& str = " \t\n\r");
	string_view trim(string_view subj, const string& str = " \t\n\r");
	string ltrimmed(string_view subj, const string& str = " \t\n\r");
	string rtrimmed(string_view subj, const string& str = " \t\n\r");
	string trimmed(string_view subj, const string& str = " \t\n\r");

	static string strtolower(string str) {
		std::transform(str.begin(), str.end(), str.begin(), [](char c) { return ::tolower(c); });
		return str;
	}
	static string strtoupper(string str) {
		std::transform(str.begin(), str.end(), str.begin(), [](char c) { return ::toupper(c); });
		return str;
	}

	string& str_replace(string& subj, const string& find, const string& repl);
	string str_replaced(string subj, const string& find, const string& repl);

	bool str_begins(string_view s, char c);
	bool str_begins(string_view s, const string& v);
	bool str_ends(string_view s, char c);
	bool str_ends(string_view s, const string& v);
}