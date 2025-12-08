#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <zmq/zmq.hpp>
#include "protos/api.pb.h"
#include "WindowManager/WindowManager.h"

namespace {

struct CompositorHandle {
  std::string pid;
};

static std::optional<std::string>
read_pidfile()
{
  std::ifstream in("/tmp/matrix-wlroots.pid");
  std::string pid;
  if (in.good()) {
    std::getline(in, pid);
    if (!pid.empty()) {
      return pid;
    }
  }
  return std::nullopt;
}

static void
kill_existing_compositor()
{
  if (auto pid = read_pidfile()) {
    std::string killCmd = "kill " + *pid + " >/dev/null 2>&1";
    std::system(killCmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string kill9 = "kill -9 " + *pid + " >/dev/null 2>&1";
    std::system(kill9.c_str());
  } else {
    std::system("pkill -f matrix-wlroots >/dev/null 2>&1");
  }
}

static CompositorHandle
start_compositor()
{
  kill_existing_compositor();

  CompositorHandle h;
  std::string cmd =
    "MATRIX_WLROOTS_BIN=./matrix-wlroots-debug MENU_PROGRAM=foot "
    "VOXEL_API_BIND=tcp://*:3345 "
    "bash -lc './launch --in-wm "
    ">/tmp/controls-test.log 2>&1 & echo $!'";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    char buf[64] = {0};
    if (fgets(buf, sizeof(buf), pipe)) {
      h.pid = std::string(buf);
      h.pid.erase(h.pid.find_last_not_of(" \n\r\t") + 1);
    }
    pclose(pipe);
  }
  if (h.pid.empty()) {
    if (auto pid = read_pidfile()) {
      h.pid = *pid;
    }
  }
  return h;
}

static bool
wait_for_log_contains(const std::string& path,
                      const std::string& needle,
                      int attempts = 60,
                      int millis = 100)
{
  for (int i = 0; i < attempts; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
      if (line.find(needle) != std::string::npos) {
        return true;
      }
    }
  }
  return false;
}

static std::string
find_window_for_pid(const std::string& pid,
                    int attempts = 120,
                    int millis = 100)
{
  std::string windowId;
  for (int i = 0; i < attempts; ++i) {
    std::string searchCmd = "xdotool search --pid " + pid + " 2>/dev/null";
    FILE* pipe = popen(searchCmd.c_str(), "r");
    if (pipe) {
      char buf[64] = {0};
      if (fgets(buf, sizeof(buf), pipe)) {
        windowId = std::string(buf);
        windowId.erase(windowId.find_last_not_of(" \n\r\t") + 1);
      }
      pclose(pipe);
    }
    if (!windowId.empty()) {
      return windowId;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
  }
  return windowId;
}

static bool
send_key_replay(const std::vector<std::pair<std::string, uint32_t>>& entries)
{
  const char* addr = std::getenv("VOXEL_API_ADDR_FOR_TEST");
  std::string endpoint = addr ? addr : "tcp://127.0.0.1:3345";
  for (int attempt = 0; attempt < 20; ++attempt) {
    try {
      zmq::context_t ctx(1);
      zmq::socket_t sock(ctx, zmq::socket_type::req);
      sock.set(zmq::sockopt::rcvtimeo, 500);
      sock.set(zmq::sockopt::sndtimeo, 500);
      sock.set(zmq::sockopt::linger, 100);
      sock.connect(endpoint);
      ApiRequest req;
      req.set_entityid(0);
      req.set_type(MessageType::KEY_REPLAY);
      for (const auto& e : entries) {
        auto* ent = req.mutable_keyreplay()->add_entries();
        ent->set_sym(e.first);
        ent->set_delay_ms(e.second);
      }
      std::string data;
      if (!req.SerializeToString(&data)) {
        return false;
      }
      zmq::message_t msg(data.size());
      memcpy(msg.data(), data.data(), data.size());
      if (!sock.send(msg, zmq::send_flags::none)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }
      zmq::message_t reply;
      auto res = sock.recv(reply, zmq::recv_flags::none);
      if (res.has_value()) {
        return true;
      }
    } catch (...) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return false;
}

static void
stop_compositor(const CompositorHandle& h)
{
  // Graceful quit via API, then fall back to signals if needed.
  {
    zmq::context_t ctx(1);
    zmq::socket_t sock(ctx, zmq::socket_type::req);
    sock.set(zmq::sockopt::rcvtimeo, 500);
    sock.set(zmq::sockopt::sndtimeo, 500);
    sock.set(zmq::sockopt::linger, 100);
    sock.connect("tcp://127.0.0.1:3345");
    ApiRequest req;
    req.set_entityid(0);
    req.set_type(MessageType::QUIT);
    std::string data;
    if (req.SerializeToString(&data)) {
      zmq::message_t msg(data.size());
      memcpy(msg.data(), data.data(), data.size());
      sock.send(msg, zmq::send_flags::none);
      zmq::message_t reply;
      sock.recv(reply, zmq::recv_flags::none);
    }
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  if (!h.pid.empty()) {
    std::string killCmd = "kill " + h.pid + " >/dev/null 2>&1";
    std::system(killCmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string kill9 = "kill -9 " + h.pid + " >/dev/null 2>&1";
    std::system(kill9.c_str());
  } else {
    std::system("pkill -f matrix-wlroots >/dev/null 2>&1");
  }
}

} // namespace

TEST(ControlsSpec, SuperEUnfocusesWindowManager)
{
  auto comp = start_compositor();
  ASSERT_FALSE(comp.pid.empty()) << "Failed to start compositor";

  // Clean logs for deterministic assertions.
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-wm.log", std::ios::trunc).close();

  // Allow API to come up.
  std::this_thread::sleep_for(std::chrono::milliseconds(4000));

  // Launch terminal via menu hotkey (v).
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu launch key";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 600, 100))
    << "Foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  // Focus looked-at app via 'r' so WM focus is set.
  ASSERT_TRUE(send_key_replay({ { "r", 0 } })) << "Failed to send focus key";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

  // Super+E should unfocus via WM and log.
  bool sent = send_key_replay({ { "Super_L", 0 }, { "e", 50 } });
  ASSERT_TRUE(sent) << "Failed to send Super+E via key replay";

  // Only wait briefly; the unfocus log should be immediate if the combo worked.
  bool sawUnfocus =
    wait_for_log_contains("/tmp/matrix-wlroots-wm.log", "WM: unfocusApp", 20, 100);
  std::this_thread::sleep_for(std::chrono::seconds(2));

  stop_compositor(comp);
  EXPECT_TRUE(sawUnfocus) << "Expected WM unfocusApp log after Super+E";
}
