#pragma once
#include "common.h"
#include "cpp/string.h"
#define ITEASE_VERSION_MAJOR 0
#define ITEASE_VERSION_MINOR 0
#define ITEASE_VERSION_BETA	0
#define ITEASE_VERSION_ALPHA 1

namespace iTease {
	struct Version {
		static const Version Latest;

		int major = ITEASE_VERSION_MAJOR, minor = ITEASE_VERSION_MINOR, beta = ITEASE_VERSION_BETA;

		// construct Version from the current version macros
		Version() = default;
		// construct Version from specified version numbers
		constexpr Version(int major, int minor = 0, int beta = 0) : major(major), minor(minor), beta(beta)
		{ }
		// construct Version from version string ([major[.minor[.beta]]])
		Version(const string& version_string) {
			std::stringstream ss(version_string);
			char c;
			if (ss >> major) {
				if (ss >> c && c == '.' && ss >> minor) {
					if (!(ss >> c) || c != '.' || !(ss >> beta))
						beta = 0;
				}
				else minor = beta = 0;
			}
			else major = minor = beta = 0;
		}

		// conversion to boolean - returns true for any version not 0.0.0
		operator bool() const { return major || minor || beta; }
		// operations for version comparison
		auto operator==(const Version& other) const->bool { return major == other.major && minor == other.minor && beta == other.beta; }
		auto operator!=(const Version& other) const->bool { return !(*this == other); }
		auto operator<(const Version& other) const->bool { return major < other.major || minor < other.minor || beta < other.beta; }
		auto operator>(const Version& other) const->bool { return major > other.major || minor > other.minor || beta > other.beta; }
		auto operator>=(const Version& other) const->bool { return !(*this < other); }
		auto operator<=(const Version& other) const->bool { return !(*this > other); }
	};
}