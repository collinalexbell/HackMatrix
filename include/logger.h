#pragma once

#include "spdlog/sinks/rotating_file_sink.h"
#include <memory>

extern std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> fileSink;

