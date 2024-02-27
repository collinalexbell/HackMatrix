#pragma once

#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <memory>
#include <vector>
#include <string>

class LoggerSink : public spdlog::sinks::base_sink<std::mutex> {
public:
  LoggerSink(spdlog::sink_ptr fileSink, spdlog::sink_ptr imguiSink) : fileSink_(fileSink), imguiSink_(imguiSink) {}
  void sink_it_(const spdlog::details::log_msg &msg) override;
  void flush_() override;
private:
  spdlog::sink_ptr fileSink_;
  spdlog::sink_ptr imguiSink_;
};

class LoggerVector {
  std::mutex _mutex;
  std::vector<std::string> msgs;
 public:
  void log(std::string msg);
  std::vector<std::string> fetch();
};

class ImGuiSink : public spdlog::sinks::base_sink<std::mutex> {
  std::shared_ptr<LoggerVector> loggerVector;

public:
  ImGuiSink(std::shared_ptr<LoggerVector> loggerVector)
      : loggerVector(loggerVector){};
  void sink_it_(const spdlog::details::log_msg &msg) override;
  void flush_() override;
};

extern std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> fileSink;
