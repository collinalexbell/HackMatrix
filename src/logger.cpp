#include <sstream>
#include "logger.h"

Loggable::Loggable(std::string loggerName, LoggerSink sink) {
  logger = make_shared<spdlog::logger>(loggerName, sink);
}

// TODO:I should make this a rotating log
std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> fileSink =
  std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/matrix.log", 50000, 0);

void LoggerSink::sink_it_(const spdlog::details::log_msg &msg) {
    // Forward to file sink
    fileSink_->log(msg);

    // Forward to ImGui sink
    imguiSink_->log(msg);
  }

void LoggerSink::flush_() {
    fileSink_->flush();
    imguiSink_->flush();
  }

void ImGuiSink::sink_it_(const spdlog::details::log_msg &msg) {
    // Check if the log level is DEBUG
    if (msg.level != spdlog::level::debug) {
      return;
    }

    std::stringstream formattedMsg;
    formattedMsg << "[" << msg.logger_name.data() << "] " << msg.payload.data() << std::endl;

    loggerVector->log(formattedMsg.str());
}

// Flush function (empty, currently flushing on every message)
void ImGuiSink::flush_() {}


void LoggerVector::log(std::string msg) {
  std::lock_guard<std::mutex> lock(_mutex);
  msgs.push_back(msg);
}

std::vector<std::string> LoggerVector::fetch() {
  std::lock_guard<std::mutex> lock(_mutex);
  return msgs;
}
