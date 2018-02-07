#pragma once
#include <any>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <variant>
#include <nlohmann/json.hpp>
#include <tsl/ordered_map.h>
#include <tsl/ordered_set.h>
#include <tsl/ordered_hash.h>
#include <cppformat/format.h>
#include <date/date.h>
#include <date/tz.h>
#include "cpp/icompare.h"
#include "cpp/string.h"
#include "cpp/contracts.h"

#define ITEASE_LOGGING
#define ITEASE_NAMESPACE_BEGIN namespace iTease {
#define ITEASE_NAMESPACE_END }

ITEASE_NAMESPACE_BEGIN
// todo: eagerly wait to remove 'experimental'
namespace fs = std::experimental::filesystem;
using namespace std::string_literals;
using namespace std::chrono_literals;

using json = nlohmann::json;

using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;
using std::string;
using std::wstring;
using std::string_view;
using std::vector;
using std::map;
using std::set;
using std::unordered_map;
using std::unordered_set;
using std::tuple;
using std::pair;
using std::any;
using std::optional;
using std::variant;
using std::enable_shared_from_this;
using std::regex;
using tsl::ordered_map;
using tsl::ordered_set;
using dynamic_bitset = vector<bool>;



template<typename Key, typename Value>
using ci_map = map<Key, Value, iless>;
template<typename Key>
using ci_set = set<Key, iless>;

template<typename Value>
using transparent_set = std::set<Value, std::less<>>;
template<typename Key, typename Value>
using transparent_map = std::map<Key, Value, std::less<>>;
template<typename Key, typename Value>
using transparent_multimap = std::multimap<Key, Value, std::less<>>;

string convert_any_to_string(const any&);

void sleep(unsigned msec);
string get_timestamp();
unsigned get_system_time();       
unsigned get_time_since_epoch();
string random_bytes(int num_bytes);
void random_bytes(int num_bytes, char* output_buffer);
string md5_hash(const string&);
string rand_string(int size, const string& charset = "abcdefghijklmnopqrstuvwxyz0123456789");
string rand_md5(int size = 12);
string rand_base64(int size);
string bytes_to_hex_string(const char* input, size_t len);

template<typename T>
T convert_any(const any&);
template<> bool convert_any(const any& var);
template<> int convert_any(const any& var);
template<> double convert_any(const any& var);
template<> string convert_any(const any& var);

template<typename T>
optional<T> any_optional_cast(const any& var) {
	T val;
	if (var >> val) return val;
	return optional<T>{};
}

template<typename T>
class Convert {
public:
	Convert(T& v) : value(v)
	{ }

	T& value;
};

template<typename T>
bool operator>>(const any& var, T& val) {
	if (var.type() == typeid(T)) {
		val = std::any_cast<T>(var);
		return true;
	}
	return false;
}
template<typename T>
bool operator>>(const any& var, Convert<T>& val) {
	return var >> val.value;
}
template<typename T>
bool operator>>(const any& var, Convert<optional<T>>& val) {
	T v;
	if (var >> Convert<T>(v)) {
		val.value = v;
	}
	return false;
}
template<typename T>
bool operator>>(const any& var, std::enable_if_t<std::is_integral_v<T>, Convert<T>>& val) {
	if (auto v = any_optional_cast<double>(var))
		val.value = (T)*v;
	else if (auto v = any_optional_cast<float>(var))
		val.value = (T)*v;
	else {
		if (auto v = any_optional_cast<string>(var)) {
			char* end;
			long long num = std::strtoll(v->c_str(), &end, 10);
			if (end == v->c_str() + v->size()) {
				val.value = (T)num;
				return true;
			}
			return false;
		}
		else {
			int v;
			if (var >> v) val.value = (T)v;
			else return false
		}
	}
	return true;
}
bool operator>>(const any& var, Convert<float>& val);
bool operator>>(const any& var, Convert<double>& val);
bool operator>>(const any& var, Convert<string>& val);
bool operator>>(const any& var, Convert<bool>& val);

template<typename T, typename Kty, typename TMap>
auto map_find(TMap& map, const Kty& key)->typename TMap::value_type::second_type* {
	auto it = map.find(key);
	return it != map.end() ? &it->second : nullptr;
}
template<typename T, typename Kty, typename TMap>
auto map_find(const TMap& map, const Kty& key)->const typename TMap::value_type::second_type* {
	auto it = map.find(key);
	return it != map.end() ? &it->second : nullptr;
}

// https://stackoverflow.com/questions/38440844/constructor-arguments-from-a-tuple
template <class R, class T, size_t... Is>
R construct_from_tuple(const T &t, std::index_sequence<Is...> = { }) {
	if (std::tuple_size<T>::value == sizeof...(Is)) {
		return {std::get<Is>(t)...};
	}
	return construct_from_tuple<R>(t, std::make_index_sequence<std::tuple_size<T>::value>{});
}
/*template<typename Tuple, std::size_t... Seq>
struct tuple_remove_refs_impl {
	using type = std::tuple<std::tuple_element_t<Seq, Tuple>...>;
};
template<typename Tuple>
struct tuple_remove_refs {
	using is = std::index_sequence<std::tuple_size<Tuple>::value>;
	using type = tuple_remove_refs_impl<Tuple, sizeof...(Tuple)>::type;
};*/
//template<typename... Ts> std::tuple<Ts...>
// https://stackoverflow.com/questions/12742877/remove-reference-from-stdtuple-members
//template<template<typename T...>>
//using tuple_no_refs = std::tuple<typename std::remove_reference<T>::type...>;
/*template <typename... T>
tuple_no_refs remove_tuple_refs(std::tuple<T...> const& t) {
	return tuple_no_refs{t};
}*/
ITEASE_NAMESPACE_END