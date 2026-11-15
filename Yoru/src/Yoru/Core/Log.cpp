#include "pch.h"
#include "Yoru/Core/Log.h"

namespace Yoru
{
	static std::ofstream g_File;
	static std::mutex g_LogMutex;
	static std::unordered_map<LogLevel, fmt::color> g_LogColors
	{
		{ LogLevel::FATAL, fmt::color::indian_red },
		{ LogLevel::ERROR, fmt::color::red },
		{ LogLevel::WARN, fmt::color::yellow },
		{ LogLevel::INFO, fmt::color::green },
		{ LogLevel::DEBUG, fmt::color::white },
		{ LogLevel::TRACE, fmt::color::white },
	};
	static std::unordered_map<LogLevel, std::string> g_LogString
	{
		{ LogLevel::FATAL, "FATAL"},
		{ LogLevel::ERROR, "ERROR"},
		{ LogLevel::WARN,  "WARN"},
		{ LogLevel::INFO,  "INFO"},
		{ LogLevel::DEBUG, "DEBUG"},
		{ LogLevel::TRACE, "TRACE"},
	};

	bool Log::Startup(const char* filePath)
	{
		Log& log = Get();

		if (!filePath || filePath[0] == '\0')
		{
			filePath = "EngineLog.txt";
		}

		g_File.open(filePath, std::ios::out | std::ios::trunc);
		if (!g_File.is_open())
		{
			Log::Write(LogLevel::ERROR, "Failed to open the log file!");
			return false;
		}
		return true;
	}

	bool Log::Shutdown()
	{
		if (g_File.is_open())
		{
			g_File.close();
			return true;
		}

		return false;
	}

	void Log::InternalWrite(LogLevel level, const std::string& msg)
	{
		if (!m_LogConsole && !m_LogFile)
			return;

		std::lock_guard<std::mutex> lock(g_LogMutex);

		if (m_LogConsole && level <= m_MinLevelConsole)
		{
			fmt::print(fmt::fg(g_LogColors[level]), 
				"{}: {}\n", g_LogString[level], msg);
		}

		if (m_LogFile && level <= m_MinLevelFile && g_File)
		{
			g_File << fmt::format("{}: {}", g_LogString[level], msg) << std::endl;
		}
	}
}
