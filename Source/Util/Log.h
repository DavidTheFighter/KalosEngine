#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <memory>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

class Log
{
public:
	Log();
	virtual ~Log();

	static std::shared_ptr<spdlog::logger> get();
	static void setInstance(Log *inst);
	static Log *getInstance();

private:
	static Log *logInstance;

	std::shared_ptr<spdlog::logger> consolePtr;
};

#endif
