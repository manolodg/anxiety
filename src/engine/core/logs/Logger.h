#pragma once

#include <chrono>
#include <format>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>

#if defined(_WIN32) | defined(_WIN64)
#include <Windows.h>
#endif

namespace anxiety::logs {
	// LogLevel - nivel de severidad de log -------------------------------------------------------
	enum class LogLevel : uint8_t {
		Trace   = 0,
		Debug   = 1,
		Info    = 2,
		Warning = 3,
		Error   = 4,
		Fatal   = 5
	};

	// LogEntry - entrada de log individual -------------------------------------------------------
	struct LogEntry {
		LogLevel                              level;
		std::string                           message;
		std::string                           category;
		std::source_location                  location;
		std::chrono::system_clock::time_point timestamp;
	};

	// Logger - registrador thread-safe con salida por consola ------------------------------------
	class Logger {
	public:
		static Logger& get() noexcept;

		void                   set_level(LogLevel level)       noexcept;
		[[nodiscard]] LogLevel get_level()               const noexcept;

		void log(LogLevel level, std::string_view category, std::string_view message, const std::source_location& location = std::source_location::current());

		template<typename... Args>
		void logf(LogLevel level, std::string_view category, std::format_string<Args...> format, Args&&... args) {
			if (level < m_level) return;

			// source_location no se reenvía a través de templates en MSVC. Usa la sobrecarga de log()
			// sin formato cuando se necesite información del punto de llamada.
			log(level, category, std::format(format, std::forward<Args>(args)...));
		}

	private:
		LogLevel   m_level{ LogLevel::Trace };
		std::mutex m_mutex;

#if defined(_WIN32) | defined(_WIN64)
		Logger() {
			// Damos soporte de UTF8 en Windows tanto consola como runtime (solo para Windows)
			SetConsoleOutputCP(CP_UTF8);
			SetConsoleCP(CP_UTF8);
		}
#else
		Logger()  = default;
#endif
		~Logger() = default;

		Logger(const Logger&)            = delete;
		Logger& operator=(const Logger&) = delete;

		void write_entry(const LogEntry& entry);

		static std::string_view level_to_string(LogLevel level) noexcept;
		static std::string_view level_color(LogLevel level)     noexcept;
	};

// Macros de uso ----------------------------------------------------------------------------------
#define LOG_TRACE(cat, msg)   ::anxiety::logs::Logger::get().log(::anxiety::logs::LogLevel::Trace,   (cat), (msg))
#define LOG_DEBUG(cat, msg)   ::anxiety::logs::Logger::get().log(::anxiety::logs::LogLevel::Debug,   (cat), (msg))
#define LOG_INFO(cat, msg)    ::anxiety::logs::Logger::get().log(::anxiety::logs::LogLevel::Info,    (cat), (msg))
#define LOG_WARNING(cat, msg) ::anxiety::logs::Logger::get().log(::anxiety::logs::LogLevel::Warning, (cat), (msg))
#define LOG_ERROR(cat, msg)   ::anxiety::logs::Logger::get().log(::anxiety::logs::LogLevel::Error,   (cat), (msg))
#define LOG_FATAL(cat, msg)   ::anxiety::logs::Logger::get().log(::anxiety::logs::LogLevel::Fatal,   (cat), (msg))

// __VA_OPT__(,) inserta la coma solo cuando __VA_ARGS__ no esta vacío.
// Este comportamiento de C++20 estándar es requerido por /Zc:preprocessor (modo de cumplimiento).
#define LOGF_INFO(cat, fmt, ...)    ::anxiety::logs::Logger::get().logf(::anxiety::logs::LogLevel::Info,    (cat), fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGF_WARNING(cat, fmt, ...) ::anxiety::logs::Logger::get().logf(::anxiety::logs::LogLevel::Warning, (cat), fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGF_ERROR(cat, fmt, ...)   ::anxiety::logs::Logger::get().logf(::anxiety::logs::LogLevel::Error,   (cat), fmt __VA_OPT__(,) __VA_ARGS__)
} // namespace anxiety::logs