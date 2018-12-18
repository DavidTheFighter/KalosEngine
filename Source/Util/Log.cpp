#include "Log.h"

#include <common.h>
#include <stdarg.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

Log *Log::logInstance = nullptr;

Log::Log()
{
	consolePtr = spdlog::stdout_color_mt("console");
	consolePtr->set_pattern("[" + std::string(ENGINE_NAME) + "] [%H:%M:%S] [T%t] %l: %v");
}

Log::~Log()
{

}

std::shared_ptr<spdlog::logger> Log::get()
{
	return logInstance->consolePtr;
}

void Log::setInstance(Log *inst)
{
	logInstance = inst;
}

Log *Log::getInstance()
{
	return logInstance;
}
