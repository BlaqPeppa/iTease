#include "stdinc.h"
#include <ctime>
#include "common.h"
#include "cpp/string.h"
#include <openssl/rand.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#ifdef _WIN32
#include <Windows.h>
#include <mmsystem.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

namespace iTease {
	// converts basic any types to string, returns empty string on failure
	string convert_any_to_string(const any& v) {
		if (v.type() == typeid(string))
			return std::any_cast<string>(v);
		if (v.type() == typeid(string_view))
			return string(std::any_cast<string_view>(v));
		if (v.type() == typeid(const char*))
			return string(std::any_cast<const char*>(v));
		if (v.type() == typeid(bool))
			return std::any_cast<bool>(v) ? "1" : "";
		if (v.type() == typeid(int))
			return std::to_string(std::any_cast<int>(v));
		if (v.type() == typeid(long))
			return std::to_string(std::any_cast<long>(v));
		if (v.type() == typeid(unsigned int))
			return std::to_string(std::any_cast<unsigned int>(v));
		if (v.type() == typeid(unsigned long))
			return std::to_string(std::any_cast<unsigned long>(v));
		if (v.type() == typeid(double))
			return std::to_string(std::any_cast<double>(v));
		if (v.type() == typeid(float))
			return std::to_string(std::any_cast<float>(v));
		return ""s;
	}

	
	bool operator>>(const any& var, Convert<float>& val) {
		if (auto v = any_optional_cast<int>(var))
			val.value = (float)*v;
		else if (auto v = any_optional_cast<double>(var))
			val.value = (float)*v;
		else {
			if (auto v = any_optional_cast<string>(var)) {
				char* end;
				double num = std::strtod(v->c_str(), &end);
				if (end == v->c_str() + v->size()) {
					val.value = (float)num;
					return true;
				}
				return false;
			}
			else
				return var >> val.value;
		}
		return true;
	}
	bool operator>>(const any& var, Convert<double>& val) {
		if (auto v = any_optional_cast<int>(var))
			val.value = (float)*v;
		else if (auto v = any_optional_cast<double>(var))
			val.value = (float)*v;
		else {
			if (auto v = any_optional_cast<string>(var)) {
				char* end;
				double num = std::strtod(v->c_str(), &end);
				if (end == v->c_str() + v->size()) {
					val.value = (float)num;
					return true;
				}
				return false;
			}
			else
				return var >> val.value;
		}
		return true;
	}
	bool operator>>(const any& var, Convert<string>& val) {
		if (auto v = any_optional_cast<int>(var))
			val.value = std::to_string(*v);
		else if (auto v = any_optional_cast<double>(var))
			val.value = std::to_string(*v);
		else if (auto v = any_optional_cast<float>(var))
			val.value = std::to_string(*v);
		else
			return var >> val.value;
		return true;
	}
	bool operator>>(const any& var, Convert<bool>& val) {
		if (var.type() != typeid(bool)) {
			int v;
			if (var >> Convert<int>{v}) {
				val.value = v != 0;
				return true;
			}
		}
		return var >> val.value;
	}

	void sleep(unsigned mSec) {
		#ifdef _WIN32
		::Sleep(mSec);
		#else
		timespec time;
		time.tv_sec = mSec / 1000;
		time.tv_nsec = (mSec % 1000) * 1000000;
		nanosleep(&time, 0);
		#endif
	}

	unsigned get_system_time() {
		#ifdef _WIN32
		return (unsigned)timeGetTime();
		#elif __EMSCRIPTEN__
		return (unsigned)emscripten_get_now();
		#else
		struct timeval time;
		gettimeofday(&time, NULL);
		return (unsigned)(time.tv_sec * 1000 + time.tv_usec / 1000);
		#endif
	}

	unsigned get_time_since_epoch() {
		return (unsigned)time(NULL);
	}

	string get_timestamp() {
		time_t t;
		time(&t);
		const char* dt = ctime(&t);
		return str_replaced(dt, "\n", "");
	}

	string bytes_to_hex_string(const char* input, size_t len) {
		static const char* const lut = "0123456789ABCDEF";
		string output;
		output.reserve(len * 2);
		for (size_t i = 0; i < len; ++i) {
			const unsigned char c = input[i];
			output.push_back(lut[c >> 4]);
			output.push_back(lut[c & 15]);
		}
		return output;
	}

	string md5_hash(const string& src) {
		unsigned char digest[MD5_DIGEST_LENGTH];
		MD5((unsigned char*)src.c_str(), src.size(), digest);
		return bytes_to_hex_string((const char*)digest, sizeof(digest));
	}

	void random_bytes(int num_bytes, char* output_buffer) {
		RAND_bytes((unsigned char*)output_buffer, num_bytes);
	}
	string random_bytes(int num_bytes) {
		string output;
		output.resize(num_bytes);
		random_bytes(num_bytes, output.data());
		return output;
	}
	string rand_string(int size, const string& charset) {
		string str;
		str.resize(size);
		random_bytes(size, str.data());
		for (auto& c : str) {
			c = charset[c % charset.size()];
		}
		return str;
	}
	string rand_md5(int size) {
		return md5_hash(random_bytes(size));
	}
	string rand_base64(int size) {
		string buff, bytes;
		buff.resize(size);

		BIO *b64, *out;
		BUF_MEM *bptr;

		// Create a base64 filter/sink
		if ((b64 = BIO_new(BIO_f_base64())) == NULL) {
			return NULL;
		}

		// Create a memory source
		if ((out = BIO_new(BIO_s_mem())) == NULL) {
			return NULL;
		}

		// Chain them
		out = BIO_push(b64, out);
		BIO_set_flags(out, BIO_FLAGS_BASE64_NO_NL);

		// Generate random bytes
		if (!RAND_bytes((unsigned char*)buff.data(), size)) {
			return NULL;
		}

		// Write the bytes
		BIO_write(out, buff.c_str(), size);
		BIO_flush(out);

		// Now remove the base64 filter
		out = BIO_pop(b64);

		// Write the null terminating character
		//BIO_write(out, "\0", 1);
		BIO_get_mem_ptr(out, &bptr);

		// Allocate memory for the output and copy it to the new location
		bytes.resize(bptr->length);
		strncpy(bytes.data(), bptr->data, bptr->length);

		// Cleanup
		BIO_set_close(out, BIO_CLOSE);
		BIO_free_all(out);
		return bytes;
	}
}