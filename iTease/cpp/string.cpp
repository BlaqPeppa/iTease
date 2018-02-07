#include "stdinc.h"
#include "../system.h"
#include "string.h"

namespace iTease {
	string_view ltrim(string_view subj, const string& str) {
		auto p = subj.find_first_not_of(str);
		if (p == string::npos) return "";
		return subj.substr(p);
	}
	string_view rtrim(string_view subj, const string& str) {
		auto p = subj.find_last_not_of(str);
		if (p != string::npos)
			return subj.substr(0, p + 1);
		return "";
	}
	string ltrimmed(string_view subj, const string& str) {
		return string(ltrim(subj, str));
	}
	string rtrimmed(string_view subj, const string& str) {
		return string(rtrim(subj, str));
	}
	string_view trim(string_view subj, const string& str) {
		return rtrim(ltrim(subj));
	}
	string trimmed(string_view subj, const string& str) {
		return string(trim(subj, str));
	}

	string& str_replace(string& subj, const string& find, const string& repl) {
		if (!subj.empty() && !find.empty()) {
			size_t pos = 0;
			while ((pos = subj.find(find, pos)) != string::npos) {
				subj.replace(pos, find.length(), repl);
				pos += repl.length();
			}
		}
		return subj;
	}
	string str_replaced(string subj, const string& find, const string& repl) {
		return str_replace(subj, find, repl);
	}

	bool str_begins(string_view s, char c) {
		return !s.empty() && s.front() == c;
	}
	bool str_begins(string_view s, const string& v) {
		if (v.empty()) return s.empty();
		if (s.size() >= v.size()) {
			return s.compare(0, v.size(), v) == 0;
		}
		return false;
	}
	bool str_ends(string_view s, char c) {
		return !s.empty() && s.back() == c;
	}
	bool str_ends(string_view s, const string& v) {
		if (v.empty()) return true;
		if (s.size() >= v.size()) {
			return s.compare(s.size() - v.size(), v.size(), v) == 0;
		}
		return false;
	}

	#ifdef _WIN32
	wstring to_wstring(const char* str) {
		size_t n = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
		wstring out;
		out.resize(n);
		MultiByteToWideChar(CP_UTF8, 0, str, -1, &out[0], n);
		return out;
	}
	string to_string(const wchar_t* str) {
		size_t n = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
		string out;
		out.resize(n);
		WideCharToMultiByte(CP_UTF8, 0, str, -1, &out[0], n, NULL, nullptr);
		return out;
	}
	wstring to_wstring(const string& str) {
		size_t n = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), nullptr, 0);
		wstring out;
		out.resize(n);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), &out[0], n);
		return out;
	}
	string to_string(const wstring& str) {
		size_t n = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.size(), nullptr, 0, nullptr, nullptr);
		string out;
		out.resize(n);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.size(), &out[0], n, NULL, nullptr);
		return out;
	}
	#else
	static wchar_t wchar_buffer[5000];
	static char char_buffer[5000];

	inline wstring to_wstring(const char * str) {
		size_t n = mbstowcs(wchar_buffer, str, sizeof(wchar_buffer));
		wstring out;
		out.resize(n);
		mbstowcs(&out[0], str, n);
		return out;
	}
	inline string to_string(const wchar_t * str) {
		size_t n = wcstombs(char_buffer, str, sizeof(char_buffer));
		string out;
		out.resize(n);
		wcstombs(&out[0], str, n);
		return out;
	}
	inline wstring to_wstring(const string& str) {
		size_t n = mbstowcs(wchar_buffer, str.c_str(), sizeof(char_buffer));
		wstring out;
		out.resize(n);
		mbstowcs(&out[0], str.c_str(), str.size());
		return out;
	}
	inline string to_string(const wstring& str) {
		size_t n = wcstombs(char_buffer, str.c_str(), str.size());
		string out;
		out.resize(n);
		wcstombs(&out[0], str.c_str(), str.size());
		return out;
	}
	#endif
}