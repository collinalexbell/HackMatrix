#include "logger.h"


// TODO:I should make this a rotating log
 std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> fileSink =
   std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/matrix.log", 50000, 0);
