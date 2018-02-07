#include "stdinc.h"
#include "common.h"
#include "logging.h"

namespace iTease {
	class DefaultLogger : public ILogger {
	public:
		DefaultLogger(std::ostream& out) : m_out(out)
		{ }

		virtual void write(LogLevel lvl, const string& msg) const override {
			string level;
			if (lvl == LogLevel::Debug)
				level = "DEBUG";
			else if (lvl == LogLevel::Info)
				level = "INFO";
			else if (lvl == LogLevel::Warning)
				level = "WARNING";
			else if (lvl == LogLevel::Error)
				level = "ERROR";

			m_out << "[" << get_timestamp() << "]: " << level << ": " << msg << std::endl;
		}
		virtual void write(LogLevel lvl, const string& msg, const string& func, const string& file, int line) const override {
			string level;
			if (lvl == LogLevel::Debug)
				level = "DEBUG";
			else if (lvl == LogLevel::Info)
				level = "INFO";
			else if (lvl == LogLevel::Warning)
				level = "WARNING";
			else if (lvl == LogLevel::Error)
				level = "ERROR";
			#ifdef ITEASE_VERBOSE_LOGGING
			m_out << "[" << get_timestamp() << "]: " << level << ": " << func << " in " << file << "(" << line << ")" << std::endl
				<< msg << std::endl << std::endl;
			#else
			m_out << "[" << get_timestamp() << "]: " << level << ": " << msg << std::endl;
			#endif
		}

	private:
		std::ostream& m_out;
	};

	Logger::Logger(LogLevel level, const ILogger& out) : m_level(level), m_logImpl(&out)
	{ }
	auto Logger::operator()(const string& message, const string& func, const string& file, int line)->void {
		if (m_logImpl) m_logImpl->write(m_level, message, func, file, line);
	}
	auto Logger::operator<<(const string& message)->Logger& {
		if (m_logImpl) m_logImpl->write(m_level, message);
		return *this;
	}
	auto Logger::set_logger(const ILogger& logger)->void {
		m_logImpl = &logger;
	}

	DefaultLogger defaultLogger(std::cout);
	DefaultLogger errorLogger(std::cerr);

	Logger& Debug() {
		static Logger logger(LogLevel::Debug, defaultLogger);
		return logger;
	}
	Logger& Info() {
		static Logger logger(LogLevel::Info, defaultLogger);
		return logger;
	}
	Logger& Warning() {
		static Logger logger(LogLevel::Warning, defaultLogger);
		return logger;
	}
	Logger& Error() {
		static Logger logger(LogLevel::Error, errorLogger);
		return logger;
	}
}