#pragma once

#include "spdlog/sinks/basic_file_sink.h"
#include <memory>

extern std::shared_ptr<spdlog::sinks::basic_file_sink_mt> fileSink;

