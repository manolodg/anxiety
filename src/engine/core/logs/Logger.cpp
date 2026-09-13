#include "Logger.h"

#include <iostream>
#include <iomanip>
#include <ctime>

namespace anxiety::logs {
	Logger& Logger::get() noexcept {
		static Logger instance;
		return instance;
	}

	void     Logger::set_level(LogLevel level)       noexcept { m_level = level; }
	LogLevel Logger::get_level()               const noexcept { return m_level; }

	void Logger::log(LogLevel level, std::string_view category, std::string_view message, const std::source_location& location) {
		if (level < m_level) return;

		LogEntry entry{
			.level     = level,
			.message   = std::string(message),
			.category  = std::string(category),
			.location  = location,
			.timestamp = std::chrono::system_clock::now()
		};

		write_entry(entry);
	}

	void Logger::write_entry(const LogEntry& entry) {
		std::lock_guard lock(m_mutex);

		// Formatear la marca de tiempo como HH:MM:SS.mmm
		const auto tp = entry.timestamp;
		const auto tt = std::chrono::system_clock::to_time_t(tp);
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;

		std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&tm_buf, &tt);
#else
		localtime_r(&tt, &tm_buf);
#endif

		const bool is_error = (entry.level >= LogLevel::Error);
		auto&      stream   = is_error ? std::cerr : std::cout;

		stream << level_color(entry.level)
			   << '['
			   << std::put_time(&tm_buf, "%H:%M:%S")
			   << '.'
			   << std::setfill('0') << std::setw(3) << ms.count()
			   << "] "
			   << std::setfill(' ') << std::setw(8) << std::left
			   << level_to_string(entry.level)
			   << " ["
			   << entry.category
			   << "] "
			   << entry.message
			   << "\033[0m"
			   << '\n';
	}

	std::string_view Logger::level_to_string(LogLevel level) noexcept {
		switch (level) {
		case LogLevel::Trace:   return "TRACE";
		case LogLevel::Debug:   return "DEBUG";
		case LogLevel::Info:    return "INFO";
		case LogLevel::Warning: return "WARNING";
		case LogLevel::Error:   return "ERROR";
		case LogLevel::Fatal:   return "FATAL";
		default:				return "UNKNOWN";
		}
	}

	std::string_view Logger::level_color(LogLevel level) noexcept {
		// Códigos de color ANSI
		switch (level) {
		case LogLevel::Trace:   return "\033[37m";			// blanco
		case LogLevel::Debug:   return "\033[36m";			// cian
		case LogLevel::Info:    return "\033[32m";			// verde
		case LogLevel::Warning: return "\033[33m";			// amarillo
		case LogLevel::Error:   return "\033[31m";			// rojo
		case LogLevel::Fatal:   return "\033[35m";			// magenta
		default:                return "\033[0m";
		}
	}
} // namespace anxiety::logs