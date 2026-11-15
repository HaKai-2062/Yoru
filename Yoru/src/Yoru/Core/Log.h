#pragma once

#include <mutex>
#include <fmt/format.h>

namespace Yoru
{
	enum class LogLevel : uint8_t
	{
		FATAL = 0,
		ERROR,
		WARN,
		INFO,
		DEBUG,
		TRACE
	};

	class Log
	{
	public:
		Log() = default;
		~Log() = default;

		static bool Startup(const char* filePath = "");
		static bool Shutdown();

		static Log& Get()
		{
			static Log s_Instance;
			return s_Instance;
		}

		static void SetMinConsoleLevel(LogLevel minLevel) { Get().m_MinLevelConsole = minLevel; }
		static void SetMinFileLevel(LogLevel minLevel) { Get().m_MinLevelFile = minLevel; }
		static void SetConsoleLogging(bool log) { Get().m_LogConsole = log; }
		static void SetFileLogging(bool log) { Get().m_LogFile = log; }
		template <typename... Args>
		static void Write(LogLevel level,
			fmt::format_string<Args...> fmt,
			Args&&... args);

	private:
		void InternalWrite(LogLevel level, const std::string& msg);

	private:
		LogLevel m_MinLevelConsole = LogLevel::TRACE;
		LogLevel m_MinLevelFile = LogLevel::TRACE;
		bool m_LogConsole = true;
		bool m_LogFile = true;
	};

	template <typename... Args>
	static void Log::Write(LogLevel level,
		fmt::format_string<Args...> fmt,
		Args&&... args)
	{
		Get().InternalWrite(level, fmt::format(fmt, std::forward<Args>(args)...));
	}
}