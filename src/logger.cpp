#include "logger.h"


// TODO:I should make this a rotating log
 std::shared_ptr<spdlog::sinks::basic_file_sink_mt> fileSink =
     std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/matrix.log");
