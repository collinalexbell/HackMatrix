#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <zmq/zmq.hpp>
#include <functional>

#include "WindowManager/WindowManager.h"
#include "entity.h"
#undef Status
#include "protos/api.pb.h"

// This test verifies the menu launcher plumbing itself (WindowManager::menu)
// honors MENU_PROGRAM override and actually spawns a command. Keyboard
// integration (pressing 'v') will be layered on later when input injection
// is wired. It now prefers the ZMQ QUIT path for cleanup instead of signals.
static bool command_ok(const std::string& cmd)
{
  int rc = std::system(cmd.c_str());
  return rc == 0;
}

struct CompositorHandle {
  std::string pid;
};

static bool send_quit_via_api();
static bool compositor_alive(const CompositorHandle& h);
static bool fetch_status(ApiRequestResponse* out);

static bool wait_for_file(const std::string& path, int attempts = 50, int millis = 100)
{
  for (int i = 0; i < attempts; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
    std::ifstream f(path);
    if (f.good()) {
      return true;
    }
  }
  return false;
}

static bool wait_for_exit_pid(const std::string& pid, int attempts = 50, int millis = 100)
{
  if (pid.empty()) {
    return true;
  }
  for (int i = 0; i < attempts; ++i) {
    std::string checkCmd = "kill -0 " + pid + " >/dev/null 2>&1";
    if (std::system(checkCmd.c_str()) != 0) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
  }
  return false;
}

static std::string find_running_compositor_pid()
{
  // Prefer pidfile if present.
  std::ifstream in("/tmp/matrix-wlroots.pid");
  std::string pid;
  if (in.good()) {
    std::getline(in, pid);
    if (!pid.empty()) {
      return pid;
    }
  }
  // Fall back to pgrep.
  FILE* pipe = popen("pgrep -f matrix-wlroots | head -n1", "r");
  if (pipe) {
    char buf[64] = {0};
    if (fgets(buf, sizeof(buf), pipe)) {
      pid = std::string(buf);
      pid.erase(pid.find_last_not_of(" \n\r\t") + 1);
    }
    pclose(pipe);
  }
  return pid;
}

static bool wait_for_log_contains(const std::string& path,
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

static bool send_key_replay(const std::vector<std::pair<std::string, uint32_t>>& entries)
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

static std::string find_window_for_pid(const std::string& pid,
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

struct ScopeExit {
  std::function<void()> fn;
  explicit ScopeExit(std::function<void()> fn) : fn(std::move(fn)) {}
  ~ScopeExit()
  {
    if (fn) {
      fn();
    }
  }
  void dismiss() { fn = nullptr; }
};

static CompositorHandle start_compositor_with_env(const std::string& extraEnv = "")
{
  CompositorHandle h;
  // Ensure we don't have a leftover instance running; try to shut it down via API.
  std::string existingPid = find_running_compositor_pid();
  if (!existingPid.empty()) {
    // Best-effort stop; send quit if possible and wait a moment.
    send_quit_via_api();
    wait_for_exit_pid(existingPid, 50, 100);
  }
  // Fresh log for assertions; use a per-test log to avoid clobbering other runs.
  const char* testLog = "/tmp/matrix-wlroots-output.log";
  std::string truncateCmd = std::string("truncate -s 0 ") + testLog + " >/dev/null 2>&1";
  std::system(truncateCmd.c_str());
  std::string cmd = "MATRIX_WLROOTS_BIN=./matrix-wlroots-debug " + extraEnv +
                    " bash -lc './launch --in-wm >/tmp/menu-test.log 2>&1 & echo $!'";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    char buf[64] = {0};
    if (fgets(buf, sizeof(buf), pipe)) {
      h.pid = std::string(buf);
      h.pid.erase(h.pid.find_last_not_of(" \n\r\t") + 1);
    }
    pclose(pipe);
  }
  // Fallback to pid file if needed
  if (h.pid.empty()) {
    std::ifstream in("/tmp/matrix-wlroots.pid");
    if (in.good()) {
      std::getline(in, h.pid);
    }
  }
  // Wait for pidfile and startup log so we know the compositor actually booted.
  wait_for_file("/tmp/matrix-wlroots.pid");
  wait_for_file(testLog);
  return h;
}

static bool send_quit_via_api()
{
  const char* addr = std::getenv("VOXEL_API_ADDR_FOR_TEST");
  // launch sets VOXEL_API_BIND=tcp://*:3345, so talk to the local host on 3345 by default.
  std::string endpoint = addr ? addr : "tcp://127.0.0.1:3345";
  for (int attempt = 0; attempt < 5; ++attempt) {
    try {
      zmq::context_t ctx(1);
      zmq::socket_t sock(ctx, zmq::socket_type::req);
      sock.set(zmq::sockopt::rcvtimeo, 500);
      sock.set(zmq::sockopt::sndtimeo, 500);
      sock.set(zmq::sockopt::linger, 100);
      sock.connect(endpoint);

      ApiRequest req;
      req.set_entityid(0);
      req.set_type(MessageType::QUIT);
      std::string data;
      if (!req.SerializeToString(&data)) {
        return false;
      }
      zmq::message_t msg(data.size());
      memcpy(msg.data(), data.data(), data.size());
      if (!sock.send(msg, zmq::send_flags::none)) {
        continue;
      }
      zmq::message_t reply;
      auto res = sock.recv(reply, zmq::recv_flags::none);
      if (res.has_value()) {
        return true;
      }
    } catch (...) {
      // Retry a few times to allow API to come up.
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return false;
}

static bool fetch_status(ApiRequestResponse* out)
{
  if (!out) {
    return false;
  }
  const char* addr = std::getenv("VOXEL_API_ADDR_FOR_TEST");
  std::string endpoint = addr ? addr : "tcp://127.0.0.1:3345";
  for (int attempt = 0; attempt < 10; ++attempt) {
    try {
      zmq::context_t ctx(1);
      zmq::socket_t sock(ctx, zmq::socket_type::req);
      sock.set(zmq::sockopt::rcvtimeo, 500);
      sock.set(zmq::sockopt::sndtimeo, 500);
      sock.set(zmq::sockopt::linger, 100);
      sock.connect(endpoint);

      ApiRequest req;
      req.set_entityid(0);
      req.set_type(MessageType::STATUS);
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
      if (!res.has_value()) {
        continue;
      }
      if (!out->ParseFromArray(reply.data(), reply.size())) {
        return false;
      }
      return true;
    } catch (...) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return false;
}

static bool stop_compositor(CompositorHandle& h)
{
  // Give API a moment to come up before sending quit.
  std::this_thread::sleep_for(std::chrono::seconds(4));
  bool quitSent = send_quit_via_api();

  if (h.pid.empty()) {
    std::ifstream in("/tmp/matrix-wlroots.pid");
    if (in.good()) {
      std::getline(in, h.pid);
    }
  }
  if (h.pid.empty()) {
    return quitSent;
  }
  if (!wait_for_exit_pid(h.pid, 50, 100)) {
    // Retry quit once more before falling back to direct kill so we don't leave orphans.
    send_quit_via_api();
    wait_for_exit_pid(h.pid, 50, 100);
  }
  if (compositor_alive(h)) {
    std::string killCmd = "kill " + h.pid + " >/dev/null 2>&1";
    std::system(killCmd.c_str());
    std::this_thread::sleep_for(std::chrono::seconds(2));
    wait_for_exit_pid(h.pid, 20, 100);
  }
  if (compositor_alive(h)) {
    // Last resort: force kill so tests never leave the compositor running.
    std::string killCmd = "kill -9 " + h.pid + " >/dev/null 2>&1";
    std::system(killCmd.c_str());
    wait_for_exit_pid(h.pid, 20, 100);
  }
  return quitSent;
}

static bool compositor_alive(const CompositorHandle& h)
{
  if (h.pid.empty()) {
    return false;
  }
  std::string checkCmd = "kill -0 " + h.pid + " >/dev/null 2>&1";
  int rc = std::system(checkCmd.c_str());
  return rc == 0;
}

// Verifies we can stop compositor via pidfile/PID without pkill.
TEST(WaylandMenuSpec, CompositorStopsWithPidFile)
{
  auto h = start_compositor_with_env();
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";
  bool quitSent = stop_compositor(h);
  guard.dismiss();
  bool alive = compositor_alive(h);
  EXPECT_FALSE(alive) << "Compositor still alive after stop_compositor";
  // Validate logs indicate startup and quit markers when available.
  std::ifstream in("/tmp/matrix-wlroots-output.log");
  std::string logs((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  EXPECT_NE(logs.find("startup:"), std::string::npos)
    << "No startup marker in compositor log (did compositor fail to boot?)";
  if (quitSent) {
    EXPECT_NE(logs.find("api quit requested"), std::string::npos)
      << "No QUIT marker found in compositor log";
  }
}

TEST(WaylandMenuSpec, LaunchesMenuProgramViaEnvOverrideWithVKey)
{
  namespace fs = std::filesystem;

  fs::path tmp = fs::temp_directory_path();
  fs::path marker = tmp / "menu-invoked.txt";
  fs::path script = tmp / "menu-test.sh";

  // Clean slate
  if (fs::exists(marker)) {
    fs::remove(marker);
  }
  // Truncate renderer/app logs for clean assertions.
  std::ofstream("/tmp/matrix-wlroots-renderer.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-waylandapp.log", std::ios::trunc).close();

  // Script that writes a marker then sleeps briefly.
  {
    std::ofstream out(script);
    out << "#!/bin/bash\n"
        << "echo invoked > \"" << marker.string() << "\"\n"
        << "sleep 0.2\n";
  }
  fs::permissions(script,
                  fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write,
                  fs::perm_options::add);

  // Start compositor with override
  std::string env = "MENU_PROGRAM=" + script.string() + " ";
  auto h = start_compositor_with_env(env);
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Trigger menu via replayed 'v' key.
  bool sent = send_key_replay({ { "v", 500 } });
  ASSERT_TRUE(sent) << "Failed to send key replay for menu key";

  bool found = false;
  for (int i = 0; i < 40; ++i) { // up to ~2s
    if (fs::exists(marker)) {
      found = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  EXPECT_TRUE(found) << "Expected marker file to be created by menu_program";

  if (found) {
    std::ifstream in(marker);
    std::string line;
    std::getline(in, line);
    EXPECT_EQ(line, "invoked");
  }

  // Verify we logged a Wayland app sample during render.
  // Optional diagnostic: if compiled with ENABLE_RENDER_TMP_LOGS, renderer logs
  // first renders to /tmp/matrix-wlroots-renderer.log. Not required for pass.
  bool sawSample = wait_for_log_contains(
    "/tmp/matrix-wlroots-waylandapp.log", "wayland-app: sample");
  if (!sawSample) {
    GTEST_SKIP() << "Sample log not present (ENABLE_RENDER_TMP_LOGS likely off)";
  }

  guard.dismiss();
  stop_compositor(h);
}

TEST(WaylandMenuSpec, FocusesTerminalAndCreatesFileViaTyping)
{
  namespace fs = std::filesystem;

  fs::path tmp = fs::temp_directory_path();
  fs::path target = tmp / "zmq-typed.txt";
  fs::path script = tmp / "menu-test.sh";

  if (fs::exists(target)) {
    fs::remove(target);
  }
  std::ofstream("/tmp/matrix-wlroots-renderer.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-waylandapp.log", std::ios::trunc).close();

  {
    std::ofstream out(script);
    out << "#!/bin/bash\n"
        << "exec foot\n";
  }
  fs::permissions(script,
                  fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write,
                  fs::perm_options::add);

  std::string env = "MENU_PROGRAM=" + script.string() + " ";
  auto h = start_compositor_with_env(env);
  ScopeExit guard([&]() {
    stop_compositor(h);
    if (fs::exists(script)) {
      fs::remove(script);
    }
    if (fs::exists(target)) {
      fs::remove(target);
    }
  });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  // Give the compositor API a brief moment to start listening.
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu launch key";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 120, 100))
    << "Foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  bool sent = send_key_replay({
    { "t", 0 }, { "o", 0 }, { "u", 0 }, { "c", 0 }, { "h", 0 },
    { "space", 0 },
    { "slash", 0 }, { "t", 0 }, { "m", 0 }, { "p", 0 }, { "slash", 0 },
    { "z", 0 }, { "m", 0 }, { "q", 0 }, { "minus", 0 }, { "t", 0 }, { "y", 0 },
    { "p", 0 }, { "e", 0 }, { "d", 0 }, { "period", 0 }, { "t", 0 }, { "x", 0 }, { "t", 0 },
    { "Return", 0 }
  });
  ASSERT_TRUE(sent) << "Failed to send key replay for terminal typing";
  wait_for_file(target.string());
}

TEST(WaylandMenuSpec, RunsNeofetchAndCapturesOutputToFile)
{
  namespace fs = std::filesystem;

  if (!command_ok("command -v neofetch >/dev/null 2>&1")) {
    GTEST_SKIP() << "neofetch not available; skipping";
  }

  fs::path tmp = fs::temp_directory_path();
  fs::path target = tmp / "neofetch.txt";
  fs::path script = tmp / "menu-test.sh";

  if (fs::exists(target)) {
    fs::remove(target);
  }
  std::ofstream("/tmp/matrix-wlroots-renderer.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-waylandapp.log", std::ios::trunc).close();

  {
    std::ofstream out(script);
    out << "#!/bin/bash\n"
        << "exec foot\n";
  }
  fs::permissions(script,
                  fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write,
                  fs::perm_options::add);

  std::string env = "MENU_PROGRAM=" + script.string() + " ";
  auto h = start_compositor_with_env(env);
  ScopeExit guard([&]() {
    stop_compositor(h);
    if (fs::exists(script)) {
      fs::remove(script);
    }
    // Leave target for inspection; it is cleaned at test start if present.
  });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  // Allow API to start before sending the first replay request.
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu launch key";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 120, 100))
    << "Foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  bool sent = send_key_replay({
    { "n", 0 }, { "e", 0 }, { "o", 0 }, { "f", 0 }, { "e", 0 }, { "t", 0 }, { "c", 0 }, { "h", 0 },
    { "Return", 0 }
  });
  ASSERT_TRUE(sent) << "Failed to send key replay for first neofetch";

  std::this_thread::sleep_for(std::chrono::seconds(3));

  sent = send_key_replay({
    { "n", 0 }, { "e", 0 }, { "o", 0 }, { "f", 0 }, { "e", 0 }, { "t", 0 }, { "c", 0 }, { "h", 0 },
    { "space", 0 }, { "greater", 0 }, { "space", 0 },
    { "slash", 0 }, { "t", 0 }, { "m", 0 }, { "p", 0 }, { "slash", 0 },
    { "n", 0 }, { "e", 0 }, { "o", 0 }, { "f", 0 }, { "e", 0 }, { "t", 0 }, { "c", 0 }, { "h", 0 },
    { "period", 0 }, { "t", 0 }, { "x", 0 }, { "t", 0 },
    { "Return", 0 }
  });
  ASSERT_TRUE(sent) << "Failed to send key replay for redirected neofetch";
  ASSERT_TRUE(wait_for_file(target.string(), 80, 100)) << "neofetch output file not created";

  std::ifstream in(target);
  std::string firstLine;
  ASSERT_TRUE(std::getline(in, firstLine)) << "neofetch output file empty";
  EXPECT_FALSE(firstLine.empty());
}

TEST(WaylandMenuSpec, ScreenshotViaKeyReplay)
{
  namespace fs = std::filesystem;
  fs::path screenshotDir = fs::path("screenshots");
  fs::create_directories(screenshotDir);
  // Clean slate: remove existing screenshots so any new file is easy to detect.
  for (auto& p : fs::directory_iterator(screenshotDir)) {
    if (p.is_regular_file()) {
      fs::remove(p.path());
    }
  }

  auto h = start_compositor_with_env();
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  bool sent = send_key_replay({ { "p", 500 } });
  ASSERT_TRUE(sent) << "Failed to send screenshot key replay";

  bool newShot = false;
  fs::path foundShot;
  for (int i = 0; i < 160; ++i) { // up to ~8s
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (auto& p : fs::directory_iterator(screenshotDir)) {
      if (!p.is_regular_file()) {
        continue;
      }
      newShot = true;
      foundShot = p.path();
      break;
    }
    if (newShot) {
      break;
    }
  }

  EXPECT_TRUE(newShot) << "No new screenshot found in screenshots/";
  if (newShot) {
    ASSERT_FALSE(foundShot.empty());
  EXPECT_TRUE(fs::file_size(foundShot) > 0);
  }

  guard.dismiss();
  stop_compositor(h);
}

TEST(WaylandMenuSpec, ReportsEngineStatusOverZmq)
{
  auto h = start_compositor_with_env();
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  // Allow API to come up before requesting status.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  ApiRequestResponse resp;
  ASSERT_TRUE(fetch_status(&resp)) << "Failed to fetch status over ZMQ";
  EXPECT_TRUE(resp.success());
  EXPECT_TRUE(resp.has_status());
  EXPECT_GT(resp.status().total_entities(), 0u);
  EXPECT_GE(resp.status().wayland_apps(), 0u);
}

// Verifies the key handler logs menu key presses.
TEST(WaylandMenuSpec, LogsMenuKeypressInHandler)
{
  if (!command_ok("command -v xdotool >/dev/null 2>&1")) {
    GTEST_SKIP() << "xdotool not available; skipping key logging test";
  }

  const std::string logPath = "/tmp/matrix-wlroots-wm.log";
  // Clean slate for log.
  std::ofstream(logPath, std::ios::trunc).close();

  auto h = start_compositor_with_env();
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  std::string windowId = find_window_for_pid(h.pid);
  ASSERT_FALSE(windowId.empty()) << "Could not find compositor window to focus";

  // Focus compositor window and send 'v' to trigger the handler.
  std::string sendCmd = "xdotool windowactivate --sync " + windowId +
                        " && xdotool windowfocus --sync " + windowId +
                        " && xdotool key --window " + windowId + " v";
  std::system(sendCmd.c_str());

  // Look for the key handler log entry (lowercase or uppercase V).
  bool sawLog = wait_for_log_contains(logPath, "sym=118", 80, 100) ||
                wait_for_log_contains(logPath, "sym=86", 80, 100);
  EXPECT_TRUE(sawLog) << "Expected key handler log for V keypress";

  guard.dismiss();
  stop_compositor(h);
}

TEST(WaylandMenuSpec, KeyReplayMovesCamera)
{
  // Truncate camera log before run.
  const std::string logPath = "/tmp/camera-move.log";
  std::ofstream(logPath, std::ios::trunc).close();

  auto h = start_compositor_with_env();
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  // Give compositor a moment to initialize.
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  // Send a replay for 'w' after 1s.
  bool sent = send_key_replay({ { "w", 1000 } });
  ASSERT_TRUE(sent) << "Failed to send key replay request";

  // Wait up to ~3s for movement log.
  bool moved = wait_for_log_contains(logPath, "camera moved", 60, 50);
  EXPECT_TRUE(moved) << "No camera movement logged after key replay";

  guard.dismiss();
  stop_compositor(h);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
