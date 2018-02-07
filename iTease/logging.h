#pragma once
#include "common.h"

namespace iTease {
	class Application;

	enum class LogLevel {
		None, Debug, Info, Warning, Error
	};

	class ILogger {
	public:
		virtual ~ILogger() { }

		virtual void write(LogLevel lvl, const string& msg) const = 0;
		virtual void write(LogLevel lvl, const string& msg, const string& func, const string& file, int line) const = 0;
	};

	class Logger {
	public:
		Logger(LogLevel, const ILogger&);
		void operator()(const string& message, const string& func, const string& file, int line);
		auto operator<<(const string& message)->Logger&;

		void set_logger(const ILogger&);

	private:
		LogLevel m_level;
		const ILogger* m_logImpl;
	};

	extern Logger& Debug();
	extern Logger& Info();
	extern Logger& Warning();
	extern Logger& Error();

	#ifdef ITEASE_LOGGING
	#define ITEASE_LOG(logger, message) logger(static_cast<std::ostringstream&>(std::ostringstream().flush() << message).str(), __FUNCTION__, __FILE__, __LINE__)
	#ifdef _DEBUG
	#define ITEASE_VERBOSE_LOGGING
	#define ITEASE_LOGDEBUG(message) ITEASE_LOG(iTease::Debug(), message)
	#else
	#define ITEASE_LOGDEBUG(message) ((void)0)
	#endif
	#define ITEASE_LOGINFO(message) ITEASE_LOG(iTease::Info(), message)
	#define ITEASE_LOGWARNING(message) ITEASE_LOG(iTease::Warning(), message)
	#define ITEASE_LOGERROR(message) ITEASE_LOG(iTease::Error(), message)
	#else
	#define ITEASE_LOGDEBUG(message) ((void)0)
	#define ITEASE_LOGINFO(message) ((void)0)
	#define ITEASE_LOGWARNING(message) ((void)0)
	#define ITEASE_LOGERROR(message) ((void)0)
	#endif
}