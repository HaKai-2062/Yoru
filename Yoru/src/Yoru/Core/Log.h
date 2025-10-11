#pragma once

#include <mutex>

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
		static void Write(LogLevel loglevel, const char* fmtStr, ...);

	private:
		void InternalWrite(LogLevel loglevel, const char* fmtStr, ...);

	private:
		LogLevel m_MinLevelConsole = LogLevel::TRACE;
		LogLevel m_MinLevelFile = LogLevel::TRACE;
		bool m_LogConsole = true;
		bool m_LogFile = true;
	};
}