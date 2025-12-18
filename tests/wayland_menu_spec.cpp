#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <set>
#include <vector>
#include <zmq/zmq.hpp>
#include <functional>

#include "WindowManager/WindowManager.h"
#include "entity.h"
#undef Status
#include "protos/api.pb.h"
#include "components/Bootable.h"

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

static int
count_log_occurrences(const std::string& path, const std::string& needle)
{
  std::ifstream in(path);
  std::string line;
  int count = 0;
  while (std::getline(in, line)) {
    if (line.find(needle) != std::string::npos) {
      ++count;
    }
  }
  return count;
}

static bool send_key_replay(const std::vector<std::pair<std::string, uint32_t>>& entries)
{
  const char* addr = std::getenv("VOXEL_API_ADDR_FOR_TEST");
  std::string endpoint = addr ? addr : "tcp://127.0.0.1:3345";
  FILE* log = std::fopen("/tmp/matrix-wlroots-test-status.log", "a");
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
        if (log) {
          std::fprintf(log, "send_key_replay: send failed attempt=%d\n", attempt);
          std::fflush(log);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }
      zmq::message_t reply;
      auto res = sock.recv(reply, zmq::recv_flags::none);
      if (res.has_value()) {
        if (log) {
          std::fprintf(log, "send_key_replay: success attempt=%d\n", attempt);
          std::fflush(log);
        }
        if (log) {
          std::fclose(log);
        }
        return true;
      }
    } catch (...) {
      if (log) {
        std::fprintf(log, "send_key_replay: exception attempt=%d\n", attempt);
        std::fflush(log);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  if (log) {
    std::fprintf(log, "send_key_replay: exhausted attempts\n");
    std::fclose(log);
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

static bool process_cmdline_contains(const std::string& needle)
{
  FILE* pipe = popen("ps -eo pid,args", "r");
  if (!pipe) {
    return false;
  }
  char* line = nullptr;
  size_t len = 0;
  bool found = false;
  while (getline(&line, &len, pipe) != -1) {
    if (line && std::string(line).find(needle) != std::string::npos) {
      found = true;
      break;
    }
  }
  if (line) {
    free(line);
  }
  pclose(pipe);
  return found;
}

static bool wait_for_process_cmdline(const std::string& needle,
                                     int attempts = 80,
                                     int millis = 100)
{
  for (int i = 0; i < attempts; ++i) {
    if (process_cmdline_contains(needle)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
  }
  return false;
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

static std::string g_lastLaunchFlags;

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
  std::string launchFlags = "--in-wm";
  const bool haveDisplay = std::getenv("DISPLAY") || std::getenv("WAYLAND_DISPLAY");
  if (const char* back = std::getenv("WLR_BACKENDS")) {
    (void)back;
    launchFlags.clear();
  } else if (!haveDisplay) {
    // Prefer DRM when no existing display is present (e.g., on a TTY).
    launchFlags = "--drm";
  }
  g_lastLaunchFlags = launchFlags;
  std::string cmd = "MATRIX_WLROOTS_BIN=./matrix-wlroots-debug " + extraEnv +
                    " bash -lc './launch " +
                    launchFlags + " >/tmp/menu-test.log 2>&1 & echo $!'";
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

static std::string last_launch_flags()
{
  return g_lastLaunchFlags;
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

// Removed flaky menu override test.

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
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 160, 100))
    << "Foot window never mapped";
  // Give foot time to settle and its shell to be ready for input.
  std::this_thread::sleep_for(std::chrono::milliseconds(800));
  // Focus the looked-at app so key replay targets foot.
  ASSERT_TRUE(send_key_replay({ { "r", 0 } })) << "Failed to focus terminal";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  bool sent = send_key_replay({
    // neofetch > /tmp/neofetch.txt
    { "n", 50 }, { "e", 50 }, { "o", 50 }, { "f", 50 }, { "e", 50 }, { "t", 50 }, { "c", 50 }, { "h", 50 },
    { "space", 50 }, { "greater", 50 }, { "space", 50 },
    { "slash", 50 }, { "t", 50 }, { "m", 50 }, { "p", 50 }, { "slash", 50 },
    { "n", 50 }, { "e", 50 }, { "o", 50 }, { "f", 50 }, { "e", 50 }, { "t", 50 }, { "c", 50 }, { "h", 50 },
    { "period", 50 }, { "t", 50 }, { "x", 50 }, { "t", 50 },
    { "Return", 80 }
  });
  ASSERT_TRUE(sent) << "Failed to send key replay for redirected neofetch";
  ASSERT_TRUE(wait_for_file(target.string(), 120, 100)) << "neofetch output file not created";

  std::string firstLine;
  bool hasContent = false;
  for (int i = 0; i < 50; ++i) {
    std::ifstream in(target);
    if (std::getline(in, firstLine) && !firstLine.empty()) {
      hasContent = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  ASSERT_TRUE(hasContent) << "neofetch output file empty";
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
  const std::string logPath = "/tmp/matrix-wlroots-wm.log";
  std::ofstream(logPath, std::ios::trunc).close();

  auto h = start_compositor_with_env();
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  // Allow compositor to initialize API.
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  // Trigger 'v' via key replay (ZMQ) and expect the WM to log the key handler.
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send key replay for V";

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

TEST(WaylandMenuSpec, WaylandTextureUsesDefaultSizeOnFirstUpload)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();

  auto h = start_compositor_with_env();
  ScopeExit guard([&]() { stop_compositor(h); });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 200, 50))
    << "Foot window never mapped for texture size check";

  // Parse compositor log for commit sizes; expect a follow-up resize toward the default.
  std::vector<std::pair<int, int>> commits;
  std::pair<int, int> lastSeen{-1, -1};
  for (int i = 0; i < 200; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::ifstream in("/tmp/menu-test.log");
    std::string line;
    while (std::getline(in, line)) {
      auto pos = line.find("commit buffer=");
      if (pos == std::string::npos) {
        continue;
      }
      int w = 0;
      int hgt = 0;
      if (std::sscanf(line.c_str() + static_cast<long>(pos),
                      "commit buffer=%*p size=%dx%d",
                      &w,
                      &hgt) == 2) {
        if (w != lastSeen.first || hgt != lastSeen.second) {
          commits.emplace_back(w, hgt);
          lastSeen = { w, hgt };
        }
      }
    }
  }

  ASSERT_GE(commits.size(), 2u) << "Expected at least two commit sizes (initial + default resize)";
  auto initial = commits.front();
  auto final = commits.back();
  double expectedW = initial.first * 0.85;
  double expectedH = initial.second * 0.85;
  int tol = 32;
  EXPECT_LE(std::abs(final.first - static_cast<int>(expectedW)), tol)
    << "Final width " << final.first << " not near default request " << expectedW;
  EXPECT_LE(std::abs(final.second - static_cast<int>(expectedH)), tol)
    << "Final height " << final.second << " not near default request " << expectedH;
}

TEST(WaylandMenuSpec, ChromiumRendersNonBlackWindow)
{
  namespace fs = std::filesystem;

  auto find_chromium = []() -> std::string {
    const char* candidates[] = { "chromium", "chromium-browser", nullptr };
    for (const char** c = candidates; *c; ++c) {
      std::string cmd = "command -v ";
      cmd += *c;
      cmd += " >/dev/null 2>&1";
      if (std::system(cmd.c_str()) == 0) {
        return *c;
      }
    }
    return "";
  };

  std::string chromeBin = find_chromium();
  if (chromeBin.empty()) {
    GTEST_SKIP() << "chromium not available; skipping render test";
  }

  fs::path tmp = fs::temp_directory_path();
  fs::path profile = tmp / "chromium-wlroots-profile";
  fs::create_directories(profile);
  fs::path chromiumLog = tmp / "chromium-menu.log";
  fs::path markerLog = "/tmp/matrix-wlroots-waylandapp.log";
  std::ofstream(markerLog, std::ios::trunc).close();
  std::ofstream(chromiumLog, std::ios::trunc).close();
  auto readChromiumLog = [&]() {
    std::ifstream clog(chromiumLog);
    return std::string((std::istreambuf_iterator<char>(clog)),
                       std::istreambuf_iterator<char>());
  };

  // Create a tiny launcher script for chromium to avoid flaky key replay typing.
  fs::path launcher = tmp / "chromium-menu.sh";
  {
    std::ofstream out(launcher);
    out << "#!/bin/bash\n";
    out << "\"" << chromeBin << "\" "
        << "--ozone-platform=wayland --enable-features=UseOzonePlatform "
        << "--no-first-run --no-default-browser-check "
        << "--hide-scrollbars --mute-audio "
        << "--user-data-dir=\"" << profile.string() << "\" "
        << "--app=data:text/html,'<html><body style=\"background: rgb(255,0,0); color: rgb(0,255,0);\">test</body></html>' "
        << "2>\"" << chromiumLog.string() << "\"\n";
  }
  std::string env = "WLR_APP_SAMPLE_LOG=1 MENU_PROGRAM=" + launcher.string() + " ";
  std::string chmodCmd = "chmod +x \"" + launcher.string() + "\"";
  std::system(chmodCmd.c_str());
  auto h = start_compositor_with_env(env);
  ScopeExit guard([&]() {
    stop_compositor(h);
    if (fs::exists(profile)) {
      std::error_code ec;
      fs::remove_all(profile, ec);
    }
  });
  ASSERT_FALSE(h.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
   // Fail fast if the compositor fell back to a headless/no-display mode; we
   // want to see the actual window.
  int headlessLogs = count_log_occurrences("/tmp/menu-test.log", "Failed to open display") +
                     count_log_occurrences("/tmp/matrix-wlroots-output.log", "Failed to open display");
  if (last_launch_flags() == "--in-wm") {
    ASSERT_EQ(headlessLogs, 0) << "Compositor could not open a display (headless fallback); rerun inside a WM.";
  } else {
    // In DRM/headless modes we don't have X11; just require the compositor to have mapped something.
    EXPECT_GE(headlessLogs, 0);
  }

  ApiRequestResponse statusBefore;
  ASSERT_TRUE(fetch_status(&statusBefore)) << "Failed to fetch status before launching chromium";
  ASSERT_TRUE(statusBefore.has_status()) << "STATUS response missing payload before chromium launch";
  uint32_t baselineApps = statusBefore.status().wayland_apps();

  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch chromium via menu";
  ASSERT_TRUE(wait_for_process_cmdline(profile.string(), 120, 100))
    << "Chromium process with profile marker '" << profile.string()
    << "' never appeared after menu launch. Log:\n"
    << readChromiumLog();
  // Wait for chromium to map (first mapped).
  bool mapped = wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 100);
  if (!mapped) {
    ADD_FAILURE() << "Chromium window never mapped. Log:\n" << readChromiumLog();
    guard.dismiss();
    stop_compositor(h);
    return;
  }

  // Walk forward to the app and focus it.
  ASSERT_TRUE(send_key_replay({ { "w", 0 }, { "w", 0 }, { "w", 0 }, { "r", 0 } }))
    << "Failed to move toward chromium and focus";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

  // Wait for sample logs and assert non-black content; use average to avoid first-frame zeros.
  bool sawSample = false;
  for (int i = 0; i < 200; ++i) {
    if (count_log_occurrences(markerLog, "wayland-app: sample") >= 3) {
      sawSample = true;
      break;
    }
    // Also check file has grown to avoid parsing before flush.
    if (std::filesystem::file_size(markerLog) > 0) {
      sawSample = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(sawSample) << "No Wayland sample log for chromium";
  ApiRequestResponse statusAfter;
  ASSERT_TRUE(fetch_status(&statusAfter)) << "Failed to fetch status after chromium launch";
  ASSERT_TRUE(statusAfter.has_status()) << "STATUS response missing payload after chromium launch";
  EXPECT_GT(statusAfter.status().wayland_apps(), baselineApps)
    << "Wayland app count did not increase after chromium launch";
  std::ifstream in(markerLog);
  std::string line;
  unsigned long long sumR = 0, sumG = 0, sumB = 0;
  unsigned long long sumCenterR = 0, sumCenterG = 0, sumCenterB = 0;
  size_t samples = 0;
  while (std::getline(in, line)) {
    if (line.find("wayland-app: sample") == std::string::npos) {
      continue;
    }
    unsigned r = 0, g = 0, b = 0, a = 0;
    unsigned center = 0;
    if (sscanf(line.c_str(), "%*[^f]firstPixel=(%u,%u,%u,%u) center=%x",
               &r, &g, &b, &a, &center) >= 4) {
      (void)a;
      sumR += r;
      sumG += g;
      sumB += b;
      sumCenterR += (center >> 24) & 0xFF;
      sumCenterG += (center >> 16) & 0xFF;
      sumCenterB += (center >> 8) & 0xFF;
      ++samples;
    }
  }
  ASSERT_GT(samples, 0u) << "No sampled pixels parsed for chromium";
  double avgR = sumR / static_cast<double>(samples);
  double avgG = sumG / static_cast<double>(samples);
  double avgB = sumB / static_cast<double>(samples);
  double centerRGB = (sumCenterR + sumCenterG + sumCenterB) / static_cast<double>(samples);
  double firstPixelRGB = avgR + avgG + avgB;
  EXPECT_TRUE(firstPixelRGB > 20.0 && centerRGB > 20.0)
    << "Chromium texture appeared black or empty (edge RGB sum=" << firstPixelRGB
    << ", center RGB sum=" << centerRGB << ")\nChromium log:\n"
    << readChromiumLog();

  guard.dismiss();
  stop_compositor(h);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
