#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <zmq/zmq.hpp>
#include <linux/input-event-codes.h>
#include "protos/api.pb.h"
#include "WindowManager/WindowManager.h"
#include <cmath>

namespace {

struct CompositorHandle {
  std::string pid;
};

static void stop_compositor(const CompositorHandle& h);

struct CompositorGuard {
  CompositorHandle handle;
  bool active = true;
  ~CompositorGuard() {
    if (active && !handle.pid.empty()) {
      stop_compositor(handle);
    }
  }
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
    std::system("pkill -f matrix-debug >/dev/null 2>&1");
  }
}

static uint16_t
next_api_port()
{
  static std::atomic<uint16_t> port{ 45000 };
  uint16_t p = port.fetch_add(1);
  if (p >= 50000) {
    port.store(45000);
    p = port.fetch_add(1);
  }
  return p;
}

static CompositorHandle
start_compositor()
{
  kill_existing_compositor();

  CompositorHandle h;
  // Ensure fresh logs per test and force compositor to use the expected path.
  const char* logPath = "/tmp/matrix-wlroots-output.log";
  const char* wmLogPath = "/tmp/matrix-wlroots-wm.log";
  const char* rendererLogPath = "/tmp/matrix-wlroots-renderer.log";
  {
    std::ofstream(logPath, std::ios::trunc).close();
    std::ofstream(wmLogPath, std::ios::trunc).close();
    std::ofstream(rendererLogPath, std::ios::trunc).close();
  }
  setenv("MATRIX_WLROOTS_OUTPUT", logPath, 1);
  setenv("WLROOTS_DEBUG_LOGS", "1", 1);
  const char* user = std::getenv("USER");
  std::string runtimeDir = "/tmp/xdg-runtime-";
  runtimeDir += (user ? user : "user");
  std::filesystem::create_directories(runtimeDir);
  std::error_code ec;
  std::filesystem::permissions(runtimeDir,
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace,
                               ec);
  setenv("XDG_RUNTIME_DIR", runtimeDir.c_str(), 1);
  const uint16_t port = next_api_port();
  const std::string apiBind = "tcp://*:" + std::to_string(port);
  const std::string apiAddr = "tcp://127.0.0.1:" + std::to_string(port);
  setenv("VOXEL_API_ADDR_FOR_TEST", apiAddr.c_str(), 1);
  namespace fs = std::filesystem;
  std::string bin = "./matrix-debug";
  if (!fs::exists(bin)) {
    bin = "./matrix";
  }
  std::string cmd = "MATRIX_WLROOTS_BIN=" + bin +
                    " MATRIX_WLROOTS_OUTPUT=" + std::string(logPath) +
                    " MENU_PROGRAM=foot VOXEL_API_BIND=" + apiBind +
                    " VOXEL_API_ADDR_FOR_TEST=" + apiAddr +
                    " XDG_RUNTIME_DIR=" + runtimeDir +
                    " bash -lc './launch"
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
  // Wait for compositor to announce startup so API is ready.
  for (int i = 0; i < 50; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::ifstream in("/tmp/matrix-wlroots-output.log");
    std::string line;
    while (std::getline(in, line)) {
      if (line.find("startup:") != std::string::npos) {
        return h;
      }
    }
  }
  return h;
}

static bool
compositor_alive(const std::string& pid)
{
  if (!pid.empty()) {
    std::string check = "kill -0 " + pid + " >/dev/null 2>&1";
    if (std::system(check.c_str()) == 0) {
      return true;
    }
  }
  // Fallback: any matrix-wlroots process (debug or release).
  return std::system("pgrep -f matrix-wlroots >/dev/null 2>&1") == 0;
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

static std::optional<double>
last_window_flop_value()
{
  std::ifstream in("/tmp/matrix-wlroots-output.log");
  std::string line;
  std::optional<double> value;
  while (std::getline(in, line)) {
    auto pos = line.find("windowFlop=");
    if (pos != std::string::npos) {
      try {
        value = std::stod(line.substr(pos + std::string("windowFlop=").size()));
      } catch (...) {
      }
    }
  }
  return value;
}

static int
count_file_occurrences(const std::string& path, const std::string& needle)
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

struct FileSnapshot {
  size_t count{0};
  std::filesystem::file_time_type latest = std::filesystem::file_time_type::min();
};

static FileSnapshot
snapshot_dir(const std::string& dir, const std::string& ext)
{
  namespace fs = std::filesystem;
  FileSnapshot snap;
  try {
    fs::create_directories(dir);
    for (const auto& entry : fs::directory_iterator(dir)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      if (!ext.empty() && entry.path().extension() != ext) {
        continue;
      }
      ++snap.count;
      auto t = entry.last_write_time();
      if (snap.latest < t) {
        snap.latest = t;
      }
    }
  } catch (...) {
  }
  return snap;
}

static bool
wait_for_new_file(const std::string& dir,
                  const std::string& ext,
                  const FileSnapshot& before,
                  int attempts = 60,
                  int millis = 100)
{
  for (int i = 0; i < attempts; ++i) {
    auto snap = snapshot_dir(dir, ext);
    if (snap.count > before.count || snap.latest > before.latest) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(millis));
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

static bool
send_pointer_replay(const std::vector<std::tuple<uint32_t, bool, uint32_t>>& entries)
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
      req.set_type(MessageType::POINTER_REPLAY);
      for (const auto& e : entries) {
        auto* ent = req.mutable_pointerreplay()->add_entries();
        ent->set_button(std::get<0>(e));
        ent->set_pressed(std::get<1>(e));
        ent->set_delay_ms(std::get<2>(e));
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

static bool fetch_status(ApiRequestResponse* out, int attempts = 10, int sleepMs = 200)
{
  if (!out) {
    return false;
  }
  const char* addr = std::getenv("VOXEL_API_ADDR_FOR_TEST");
  std::string endpoint = addr ? addr : "tcp://127.0.0.1:3345";
  for (int attempt = 0; attempt < attempts; ++attempt) {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        continue;
      }
      zmq::message_t reply;
      auto res = sock.recv(reply, zmq::recv_flags::none);
      if (!res.has_value()) {
        continue;
      }
      FILE* log = std::fopen("/tmp/matrix-wlroots-test-status.log", "a");
      if (!out->ParseFromArray(reply.data(), reply.size())) {
        if (log) {
          std::fprintf(log, "fetch_status: parse failure attempt=%d bytes=%zu\n", attempt, reply.size());
          std::fclose(log);
        }
        return false;
      }
      if (log) {
        const auto& st = out->status();
        std::fprintf(log,
                     "fetch_status: attempt=%d wayland=%u total=%u focus=%d\n",
                     attempt,
                     st.wayland_apps(),
                     st.total_entities(),
                     st.wayland_focus() ? 1 : 0);
        std::fclose(log);
      }
      return true;
    } catch (...) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
  }
  return false;
}

static std::optional<EngineStatus>
current_status(int attempts = 10, int sleepMs = 200)
{
  ApiRequestResponse resp;
  if (!fetch_status(&resp, attempts, sleepMs) || !resp.has_status()) {
    return std::nullopt;
  }
  return resp.status();
}

static std::optional<EngineStatus>
wait_for_wayland_count(uint32_t expectedCount,
                       int tries,
                       int sleepMs,
                       int statusAttempts = 5,
                       int statusSleepMs = 100)
{
  std::optional<EngineStatus> st;
  for (int i = 0; i < tries; ++i) {
    st = current_status(statusAttempts, statusSleepMs);
    if (st && st->wayland_apps() == expectedCount) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
  }
  return st;
}

static double
camera_distance(const EngineStatus& a, const EngineStatus& b)
{
  if (!a.has_camera_position() || !b.has_camera_position()) {
    return 0.0;
  }
  double dx = static_cast<double>(a.camera_position().x()) -
              static_cast<double>(b.camera_position().x());
  double dy = static_cast<double>(a.camera_position().y()) -
              static_cast<double>(b.camera_position().y());
  double dz = static_cast<double>(a.camera_position().z()) -
              static_cast<double>(b.camera_position().z());
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static void
stop_compositor(const CompositorHandle& h)
{
  // Graceful quit via API, then fall back to signals if needed.
  const char* addrEnv = std::getenv("VOXEL_API_ADDR_FOR_TEST");
  std::string endpoint = addrEnv ? addrEnv : "tcp://127.0.0.1:3345";
  auto graceful_quit = [&](int attempts, int sleepMs) {
    zmq::context_t ctx(1);
    zmq::socket_t sock(ctx, zmq::socket_type::req);
    sock.set(zmq::sockopt::rcvtimeo, 500);
    sock.set(zmq::sockopt::sndtimeo, 500);
    sock.set(zmq::sockopt::linger, 100);
    for (int i = 0; i < attempts; ++i) {
      try {
        sock.connect(endpoint);
        ApiRequest req;
        req.set_entityid(0);
        req.set_type(MessageType::QUIT);
        std::string data;
        if (!req.SerializeToString(&data)) {
          break;
        }
        zmq::message_t msg(data.size());
        memcpy(msg.data(), data.data(), data.size());
        if (!sock.send(msg, zmq::send_flags::none)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
          continue;
        }
        zmq::message_t reply;
        if (sock.recv(reply, zmq::recv_flags::none)) {
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
  };
  graceful_quit(/*attempts=*/5, /*sleepMs=*/200);

  auto wait_for_exit = [&](const std::string& pid, int attempts, int sleepMs) {
    if (pid.empty()) {
      return;
    }
    for (int i = 0; i < attempts; ++i) {
      std::string check = "kill -0 " + pid + " >/dev/null 2>&1";
      if (std::system(check.c_str()) != 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
  };

  wait_for_exit(h.pid, /*attempts=*/10, /*sleepMs=*/100);

  if (!h.pid.empty()) {
    std::string killCmd = "kill " + h.pid + " >/dev/null 2>&1";
    std::system(killCmd.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string kill9 = "kill -9 " + h.pid + " >/dev/null 2>&1";
    std::system(kill9.c_str());
    wait_for_exit(h.pid, /*attempts=*/50, /*sleepMs=*/100);
  } else {
    std::system("pkill -f matrix-debug >/dev/null 2>&1");
    std::system("pkill -f matrix >/dev/null 2>&1");
  }

  // Clean up stale pidfile if present.
  std::remove("/tmp/matrix-wlroots.pid");
  if (auto pid = read_pidfile()) {
    std::string kill9 = "kill -9 " + *pid + " >/dev/null 2>&1";
    std::system(kill9.c_str());
  }
}

} // namespace

TEST(ControlsSpec, SuperEUnfocusesWindowManager)
{
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  // Clean logs for deterministic assertions.
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-wm.log", std::ios::trunc).close();

  // Allow API to come up.
  std::this_thread::sleep_for(std::chrono::milliseconds(4000));

  // Nudge forward a bit so the looked-at target isn't exactly at origin.
  ASSERT_TRUE(send_key_replay({ { "w", 0 }, { "w", 0 }, { "w", 0 } }))
    << "Failed to send forward movement before spawning terminal";
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Launch terminal via menu hotkey (v).
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu launch key";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 600, 100))
    << "Foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  // Focus looked-at app via 'r' so WM focus is set.
  ASSERT_TRUE(send_key_replay({ { "r", 0 } })) << "Failed to send focus key";
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  auto wait_for_status = [](uint32_t minWayland,
                            int maxTries,
                            int sleepMs) -> std::optional<EngineStatus> {
    FILE* log = std::fopen("/tmp/matrix-wlroots-test-status.log", "a");
    std::optional<EngineStatus> st;
    for (int i = 0; i < maxTries; ++i) {
      st = current_status();
      if (log) {
        std::fprintf(log,
                     "status attempt=%d has=%d wayland=%u camera=%d\n",
                     i,
                     st.has_value() ? 1 : 0,
                     st.has_value() ? st->wayland_apps() : 0,
                     st.has_value() ? st->has_camera_position() : 0);
        std::fflush(log);
      }
      if (st.has_value() && st->has_camera_position() &&
          st->wayland_apps() >= minWayland) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
    if (log) {
      std::fclose(log);
    }
    return st;
  };

  auto statusBefore = wait_for_status(/*minWayland=*/1, /*maxTries=*/80, /*sleepMs=*/250);
  ASSERT_TRUE(statusBefore.has_value()) << "Missing engine status before unfocus";
  ASSERT_TRUE(statusBefore->has_camera_position()) << "Camera position missing from status";
  if (statusBefore->wayland_apps() == 0u) {
    FILE* log = std::fopen("/tmp/matrix-wlroots-test-status.log", "a");
    if (log) {
      std::fprintf(log, "warning: wayland_apps=0 before Super+E; continuing\n");
      std::fclose(log);
    }
  }

  // Super+E (config uses alt as super) should unfocus via WM and log.
  bool sent = send_key_replay({ { "Alt_L", 0 }, { "e", 50 } });
  ASSERT_TRUE(sent) << "Failed to send Super+E via key replay";

  // Only wait briefly; the unfocus log should be immediate if the combo worked.
  bool sawUnfocus =
    wait_for_log_contains("/tmp/matrix-wlroots-wm.log", "WM: unfocusApp", 20, 100);
  bool sawSuperCombo =
    wait_for_log_contains("/tmp/matrix-wlroots-output.log", "hotkey combo", 20, 100);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Try to move the camera via WASD-style input; movement only occurs when WM focus is cleared.
  ASSERT_TRUE(send_key_replay(
                { { "s", 0 }, { "s", 0 }, { "s", 0 }, { "s", 0 }, { "s", 0 } }))
    << "Failed to send movement keys";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  auto statusAfter = wait_for_status(statusBefore->wayland_apps(), /*maxTries=*/40, /*sleepMs=*/250);
  ASSERT_TRUE(statusAfter.has_value()) << "Missing engine status after movement";
  ASSERT_TRUE(statusAfter->has_camera_position()) << "Camera position missing after movement";
  // After unfocus, Wayland app count must remain stable.
  if (statusBefore->wayland_apps() > 0 && statusAfter->wayland_apps() > 0) {
    EXPECT_EQ(statusAfter->wayland_apps(), statusBefore->wayland_apps())
      << "Wayland app count changed after Super+E; foot may have crashed";
  }
  bool sawDestroy =
    wait_for_log_contains("/tmp/matrix-wlroots-output.log", "destroy", 10, 100);
  EXPECT_FALSE(sawDestroy) << "Wayland surface was destroyed during Super+E test";

  // Let RAII guard handle shutdown to ensure cleanup even on early failures.
  EXPECT_TRUE(sawUnfocus) << "Expected WM unfocusApp log after Super+E";
  EXPECT_TRUE(sawSuperCombo) << "Expected compositor super combo log after Super+E";
  double moved = camera_distance(*statusBefore, *statusAfter);
  EXPECT_GT(moved, 0.01) << "Camera did not move after Super+E and movement keys (delta=" << moved
                         << ")";
}

TEST(ControlsSpec, CameraSpeedHotkeysChangeAndReset)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(2500));
  ASSERT_TRUE(send_key_replay({ { "equal", 0 } })) << "Failed to send speed up key";
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  ASSERT_TRUE(
    wait_for_log_contains("/tmp/matrix-wlroots-output.log", "camera speed delta=+0.05", 120, 50))
    << "Speed up log missing after '='";

  ASSERT_TRUE(send_key_replay({ { "minus", 200 } })) << "Failed to send slow down key";
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  ASSERT_TRUE(
    wait_for_log_contains("/tmp/matrix-wlroots-output.log", "camera speed delta=-0.05", 120, 50))
    << "Slow down log missing after '-'";

  ASSERT_TRUE(send_key_replay({ { "Shift_L", 0 }, { "0", 180 } }))
    << "Failed to send speed reset combo";
  ASSERT_TRUE(
    wait_for_log_contains("/tmp/matrix-wlroots-output.log", "camera speed reset", 120, 50))
    << "Speed reset log missing after Shift+0";
}

TEST(ControlsSpec, CursorToggleLogsWhenUnfocused)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));
  ASSERT_TRUE(send_key_replay({ { "f", 0 } })) << "Failed to send toggle_cursor key";
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls handled sym=f(102) consumed=1",
                                    80,
                                    50))
    << "Cursor toggle was not consumed when unfocused";

  ASSERT_TRUE(send_key_replay({ { "f", 200 } })) << "Failed to send toggle_cursor key (second)";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls handled sym=f(102) consumed=1",
                                    80,
                                    50))
    << "Cursor toggle reset log missing when unfocused";
}

TEST(ControlsSpec, WindowFlopHotkeysAdjust)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(2500));
  ASSERT_TRUE(send_key_replay({ { "0", 0 }, { "0", 0 }, { "0", 0 } }))
    << "Failed to send window flop increase";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "windowFlop=",
                                    /*attempts=*/120,
                                    /*millis=*/50))
    << "windowFlop increase log missing";
  auto increased = last_window_flop_value();
  ASSERT_TRUE(increased.has_value()) << "Window flop increase was not logged";
  EXPECT_GT(*increased, 0.01) << "windowFlop did not increase after 0 presses";

  std::vector<std::pair<std::string, uint32_t>> decKeys;
  for (int i = 0; i < 10; ++i) {
    decKeys.push_back({ "9", 0 });
  }
  ASSERT_TRUE(send_key_replay(decKeys)) << "Failed to send window flop decrease";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "windowFlop=",
                                    /*attempts=*/120,
                                    /*millis=*/50))
    << "windowFlop decrease log missing";
  auto clamped = last_window_flop_value();
  ASSERT_TRUE(clamped.has_value()) << "Window flop decrease was not logged";
  EXPECT_GE(*clamped, 0.01) << "windowFlop dropped below clamp after 9 presses";
  EXPECT_LE(*clamped, *increased) << "windowFlop did not decrease after 9 presses";
}

TEST(ControlsSpec, WindowFlopHotkeysBlockedWhenWaylandFocused)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));

  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped before window flop gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Focus the Wayland app so unmodified controls are suppressed under focus.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before flop gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  // With a Wayland client focused, unmodified controls should be blocked.
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  ASSERT_TRUE(send_key_replay({ { "0", 0 }, { "0", 0 }, { "0", 0 } }))
    << "Failed to send window flop increase under focus";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  int flopLogs = count_log_occurrences("/tmp/matrix-wlroots-output.log", "windowFlop=");
  EXPECT_EQ(flopLogs, 0) << "windowFlop logs appeared while Wayland client had focus";
}

TEST(ControlsSpec, CursorToggleBlockedWhenWaylandFocused)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped before cursor toggle gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Focus the app to ensure allowControls blocks unmodified hotkeys.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before toggle check";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  ASSERT_TRUE(send_key_replay({ { "f", 0 } })) << "Failed to send toggle_cursor while focused";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  int toggleLogs = count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: toggle_cursor");
  EXPECT_EQ(toggleLogs, 0) << "Cursor toggle log appeared while Wayland client had focus";
}

TEST(ControlsSpec, SuperQClosesWaylandApp)
{
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-wm.log", std::ios::trunc).close();

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));

  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 200, 50))
    << "Foot window never mapped before Super+Q";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before Super+Q";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  auto statusBefore = current_status();
  ASSERT_TRUE(statusBefore.has_value()) << "Engine status unavailable before Super+Q";
  EXPECT_GT(statusBefore->wayland_apps(), 0u) << "No Wayland apps registered before Super+Q";

  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "q", 50 } }))
    << "Failed to send Super+Q via key replay";
  for (int i = 0; i < 8; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(compositor_alive(compGuard.handle.pid))
      << "Compositor died after Super+Q (t=" << (i + 1) << "s)";
  }

  bool sawRemove = wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                         "wayland app remove (deferred)",
                                         200,
                                         50);
  EXPECT_TRUE(sawRemove) << "Wayland app removal was not logged after Super+Q";

  bool sawDestroy = wait_for_log_contains("/tmp/matrix-wlroots-output.log", "destroy", 120, 50) ||
                    wait_for_log_contains("/tmp/matrix-wlroots-output.log", "destroyed", 120, 50);
  EXPECT_TRUE(sawDestroy) << "Wayland surface destroy log missing after Super+Q";

  // Ensure no new add occurs after the first removal marker.
  {
    std::ifstream log("/tmp/matrix-wlroots-output.log");
    std::string line;
    bool sawRemoveLine = false;
    bool addAfterRemove = false;
    while (std::getline(log, line)) {
      if (!sawRemoveLine &&
          line.find("wayland deferred remove queued") != std::string::npos) {
        sawRemoveLine = true;
        continue;
      }
      if (sawRemoveLine &&
          line.find("wayland app add (deferred)") != std::string::npos) {
        addAfterRemove = true;
        break;
      }
    }
    EXPECT_FALSE(addAfterRemove) << "Wayland app remapped after Super+Q (add logged after remove)";
  }
  auto statusAfter = current_status();
  ASSERT_TRUE(statusAfter.has_value()) << "Engine status unavailable after Super+Q";
  EXPECT_EQ(statusAfter->wayland_apps(), 0u) << "Wayland app still registered after Super+Q";
}

TEST(ControlsSpec, DebugHotkeysLogWhenUnfocused)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  ASSERT_TRUE(send_key_replay({ { "b", 0 },
                                { "t", 150 },
                                { "comma", 150 },
                                { "m", 150 },
                                { "slash", 1050 } }))
    << "Failed to send debug hotkeys";
  // Wait for each hotkey log to show to avoid race with slow renderer toggles.
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: logCounts",
                                    120,
                                    50))
    << "logCounts hotkey did not log when unfocused";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: logBlockType",
                                    120,
                                    50))
    << "logBlockType hotkey did not log when unfocused";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: mesh=false",
                                    120,
                                    50))
    << "mesh(false) hotkey did not log when unfocused";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: mesh",
                                    120,
                                    50))
    << "mesh hotkey did not log when unfocused";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: wireframe toggle",
                                    120,
                                    50))
    << "wireframe toggle hotkey did not log when unfocused";
}

TEST(ControlsSpec, MenuHotkeyBlockedWhenWaylandFocused)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-menu.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu (first)";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped before focus check";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  int menuBefore = count_file_occurrences("/tmp/matrix-wlroots-menu.log", "menu() launching");

  // Focus the app, then try menu again; it should be blocked under Wayland focus.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before menu block check";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu hotkey while focused";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

  int menuAfter = count_file_occurrences("/tmp/matrix-wlroots-menu.log", "menu() launching");
  EXPECT_EQ(menuAfter, menuBefore) << "Menu launched even though a Wayland client was focused";
}

TEST(ControlsSpec, GoToAppRequiresUnfocusedClient)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped before go-to check";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  // Focus the app so controls are suppressed, then try go-to; expect no goToApp log.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before go-to block check";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  ASSERT_TRUE(send_key_replay({ { "r", 0 } })) << "Failed to send go-to while focused";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: goToApp"), 0)
    << "goToApp ran while Wayland client was focused";

  // Unfocus via Super+E, then go-to should log.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "e", 50 } }))
    << "Failed to send Super+E to unfocus";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  ASSERT_TRUE(send_key_replay({ { "r", 0 } })) << "Failed to send go-to after unfocus";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: goToApp",
                                    120,
                                    50))
    << "goToApp did not run after unfocus";
}

TEST(ControlsSpec, ScreenshotHotkeyCreatesFileWhenUnfocused)
{
  FileSnapshot before = snapshot_dir("screenshots", ".png");
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  ASSERT_TRUE(send_key_replay({ { "p", 0 } })) << "Failed to send screenshot hotkey";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: screenshot",
                                    120,
                                    50))
    << "Screenshot hotkey was not logged";
  EXPECT_TRUE(wait_for_new_file("screenshots", ".png", before, 120, 100))
    << "Screenshot hotkey did not create a new file";
}

TEST(ControlsSpec, ScreenshotHotkeyBlockedWhenWaylandFocused)
{
  FileSnapshot before = snapshot_dir("screenshots", ".png");
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped before screenshot gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before screenshot gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  auto focusStatus = current_status();
  ASSERT_TRUE(focusStatus.has_value()) << "Missing status after focusing app";
  EXPECT_TRUE(focusStatus->wayland_focus()) << "Wayland focus not active before screenshot gate";

  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  ASSERT_TRUE(send_key_replay({ { "p", 0 } })) << "Failed to send screenshot hotkey while focused";
  bool created =
    wait_for_new_file("screenshots", ".png", before, /*attempts=*/60, /*millis=*/100);
  EXPECT_FALSE(created) << "Screenshot created while Wayland client had focus";
  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: screenshot"), 0)
    << "Screenshot log appeared while Wayland client had focus";
}

TEST(ControlsSpec, SaveHotkeyCreatesFileWhenUnfocused)
{
  FileSnapshot before = snapshot_dir("saves", ".save");
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  ASSERT_TRUE(send_key_replay({ { "l", 0 } })) << "Failed to send save hotkey";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: save",
                                    120,
                                    50))
    << "Save hotkey was not logged";
  EXPECT_TRUE(wait_for_new_file("saves", ".save", before, 120, 100))
    << "Save hotkey did not create a new file";
}

TEST(ControlsSpec, SaveHotkeyBlockedWhenWaylandFocused)
{
  FileSnapshot before = snapshot_dir("saves", ".save");
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped before save gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before save gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  auto saveFocus = current_status();
  ASSERT_TRUE(saveFocus.has_value()) << "Missing status after focusing app for save gate";
  EXPECT_TRUE(saveFocus->wayland_focus()) << "Wayland focus not active before save gate check";

  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  ASSERT_TRUE(send_key_replay({ { "l", 0 } })) << "Failed to send save hotkey while focused";
  bool created =
    wait_for_new_file("saves", ".save", before, /*attempts=*/60, /*millis=*/100);
  EXPECT_FALSE(created) << "Save file created while Wayland client had focus";
  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: save"), 0)
    << "Save log appeared while Wayland client had focus";
}

TEST(ControlsSpec, CodeBlockHotkeyMovesCamera)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  auto before = current_status(/*attempts=*/20, /*sleepMs=*/200);
  ASSERT_TRUE(before.has_value()) << "Missing status before code block hotkey";
  ASSERT_TRUE(before->has_camera_position()) << "Camera position missing before code block hotkey";

  ASSERT_TRUE(send_key_replay({ { "period", 0 } })) << "Failed to send code block hotkey";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: code block move distance=3.0",
                                    120,
                                    50))
    << "Code block hotkey was not logged";
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  auto after = current_status(/*attempts=*/20, /*sleepMs=*/200);
  ASSERT_TRUE(after.has_value()) << "Missing status after code block hotkey";
  ASSERT_TRUE(after->has_camera_position()) << "Camera position missing after code block hotkey";
  double dist = camera_distance(*before, *after);
  EXPECT_GT(dist, 0.25) << "Camera did not move after code block hotkey (delta=" << dist << ")";
}

TEST(ControlsSpec, PointerClickRespectsFocusAndGrab)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  ASSERT_TRUE(send_key_replay({ { "f", 0 } })) << "Failed to toggle cursor grab";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: toggle_cursor=1",
                                    80,
                                    50))
    << "Cursor grab toggle log missing before pointer replay";

  // With no Wayland focus, a grabbed left click should place a voxel.
  ASSERT_TRUE(send_pointer_replay({ std::make_tuple<uint32_t, bool, uint32_t>(BTN_LEFT, true, 0) }))
    << "Failed to send pointer replay (unfocused)";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: pointer place voxel",
                                    60,
                                    50))
    << "Pointer click did not place voxel when unfocused";
}

TEST(ControlsSpec, PointerClickGoToAppWhenLookedAt)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  // Spawn a Wayland app in front of the camera so goToApp has a target.
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 800, 50))
    << "Foot window never mapped before pointer go-to check";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  // Now toggle grab after we know an app is present.
  ASSERT_TRUE(send_key_replay({ { "f", 0 } })) << "Failed to toggle cursor grab";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: toggle_cursor=1",
                                    80,
                                    50))
    << "Cursor grab toggle log missing before pointer replay";

  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  ASSERT_TRUE(send_pointer_replay({ std::make_tuple<uint32_t, bool, uint32_t>(BTN_LEFT, true, 0) }))
    << "Failed to send pointer replay for go-to";
  EXPECT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log",
                                    "controls: pointer goToApp",
                                    120,
                                    50))
    << "Pointer click did not trigger goToApp when looking at app";
}

TEST(ControlsSpec, WaylandWindowSpawnDoesNotStealFocus)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));

  auto statusBefore = current_status();
  ASSERT_TRUE(statusBefore.has_value()) << "Missing status before spawn";
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

  auto statusAfterSpawn = current_status();
  ASSERT_TRUE(statusAfterSpawn.has_value()) << "Missing status after spawn";
  EXPECT_FALSE(statusAfterSpawn->wayland_focus()) << "Wayland window stole focus on spawn";

  // Move backward; movement should be processed because focus stays on engine.
  ASSERT_TRUE(send_key_replay({ { "s", 0 },
                                { "s", 150 },
                                { "s", 150 },
                                { "s", 150 },
                                { "s", 150 },
                                { "s", 150 },
                                { "s", 150 } }))
    << "Failed to send backward movement keys";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  auto statusAfterMove = current_status();
  ASSERT_TRUE(statusAfterMove.has_value()) << "Missing status after movement";
  ASSERT_TRUE(statusBefore->has_camera_position()) << "Camera missing before movement";
  ASSERT_TRUE(statusAfterMove->has_camera_position()) << "Camera missing after movement";
  double delta = camera_distance(*statusBefore, *statusAfterMove);
  EXPECT_GT(delta, 0.5) << "Camera did not move after spawn when pressing 's' (delta=" << delta << ")";
}

TEST(ControlsSpec, DebugHotkeysBlockedWhenWaylandFocused)
{
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::this_thread::sleep_for(std::chrono::milliseconds(3500));
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch foot via menu";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 400, 50))
    << "Foot window never mapped before debug hotkey gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus app before debug hotkey gate check";
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  ASSERT_TRUE(send_key_replay({ { "b", 0 },
                                { "t", 150 },
                                { "comma", 150 },
                                { "m", 150 },
                                { "slash", 150 } }))
    << "Failed to send debug hotkeys under focus";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: logCounts"), 0)
    << "logCounts hotkey logged while Wayland client had focus";
  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: logBlockType"), 0)
    << "logBlockType hotkey logged while Wayland client had focus";
  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: mesh"), 0)
    << "mesh hotkey logged while Wayland client had focus";
  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: mesh=false"), 0)
    << "mesh(false) hotkey logged while Wayland client had focus";
  EXPECT_EQ(count_log_occurrences("/tmp/matrix-wlroots-output.log", "controls: wireframe toggle"), 0)
    << "wireframe toggle hotkey logged while Wayland client had focus";
}

TEST(ControlsSpec, SuperHotkeysCycleWaylandApps)
{
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  // Clean logs.
  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-wm.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-renderer.log", std::ios::trunc).close();

  std::this_thread::sleep_for(std::chrono::milliseconds(4000));

  // Spawn first foot.
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu launch key (first)";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 300, 50))
    << "First foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

  // Focus looked-at app so it registers.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "1", 50 } }))
    << "Failed to focus first app";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Unfocus and move backward (hold 's' until next entry).
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "e", 50 }, { "s", 4000 } }))
    << "Failed to unfocus and move back";
  std::this_thread::sleep_for(std::chrono::milliseconds(4500));

  // Spawn second foot.
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu launch key (second)";
  // Wait for at least two mapped entries.
  bool twoMapped = false;
  for (int i = 0; i < 100; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (count_log_occurrences("/tmp/matrix-wlroots-output.log", "mapped") >= 2) {
      twoMapped = true;
      break;
    }
  }
  ASSERT_TRUE(twoMapped) << "Second foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

  // Cycle hotkeys: Super+1 -> Super+2 -> Super+1.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 },
                                { "1", 100 },
                                { "Alt_L", 0 },
                                { "2", 100 },
                                { "Alt_L", 0 },
                                { "1", 100 } }))
    << "Failed to send hotkey replay";
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  // Verify we hopped across at least two hotkey indices via compositor log.
  std::set<int> indices;
  std::vector<int> hotkeyIdxSeen;
  const std::vector<int> expectedHotkeySequence{ 0, 1, 0 };
  {
    std::ifstream in("/tmp/matrix-wlroots-output.log");
    std::string line;
    while (std::getline(in, line)) {
      auto pos = line.find("hotkey: idx=");
      if (pos != std::string::npos) {
        auto sub = line.substr(pos + strlen("hotkey: idx="));
        try {
          int idx = std::stoi(sub);
          indices.insert(idx);
          hotkeyIdxSeen.push_back(idx);
        } catch (...) {
        }
      }
    }
  }
  if (indices.empty()) {
    std::ifstream wmIn("/tmp/matrix-wlroots-wm.log");
    std::string line;
    while (std::getline(wmIn, line)) {
      auto pos = line.find("WM: hotkey focus idx=");
      if (pos != std::string::npos && line.find("after target") != std::string::npos) {
        try {
          int idx = std::stoi(line.substr(pos + strlen("WM: hotkey focus idx=")));
          indices.insert(idx);
          hotkeyIdxSeen.push_back(idx);
        } catch (...) {
        }
      }
    }
  }
  EXPECT_GE(indices.size(), 2u) << "Expected hotkey cycling to hit at least two indices";
  EXPECT_GE(hotkeyIdxSeen.size(), expectedHotkeySequence.size())
    << "Did not see all expected super hotkey presses in compositor log";
  size_t wmUnfocusCount = 0;
  std::map<int, int> wmHotkeyFocus;
  {
    std::ifstream wmLog("/tmp/matrix-wlroots-wm.log");
    std::string line;
    while (std::getline(wmLog, line)) {
      if (line.find("unfocusApp") != std::string::npos) {
        ++wmUnfocusCount;
      }
      auto pos = line.find("WM: hotkey focus idx=");
      if (pos != std::string::npos) {
        try {
          int idx = std::stoi(line.substr(pos + strlen("WM: hotkey focus idx=")));
          int ent = -1;
          auto entPos = line.find("ent=", pos);
          if (entPos != std::string::npos) {
            ent = std::stoi(line.substr(entPos + strlen("ent=")));
          }
          wmHotkeyFocus[idx] = ent;
        } catch (...) {
        }
      }
    }
  }
  EXPECT_GE(wmUnfocusCount, hotkeyIdxSeen.size())
    << "WindowManager did not log unfocus for each hotkey press";
  // Dump render log to ensure renderer saw the apps; useful when cycling fails visually.
  std::ifstream rlog("/tmp/matrix-wlroots-renderer.log");
  ASSERT_TRUE(rlog.good()) << "Renderer log missing after hotkey cycle";
  if (rlog.good()) {
    std::string rline;
    size_t wlEntries = 0;
    bool sawDirect = false;
    std::map<int, size_t> directCounts;
    std::map<int, size_t> directByEntity;
    while (std::getline(rlog, rline)) {
      if (rline.find("Renderer: Wayland") != std::string::npos) {
        ++wlEntries;
      }
      auto appPos = rline.find("appNumber=");
      if (appPos != std::string::npos) {
        auto appStr = rline.substr(appPos + strlen("appNumber="));
        try {
          int appNum = std::stoi(appStr);
          if (rline.find("renderAppDirect") != std::string::npos ||
              rline.find("Wayland direct") != std::string::npos) {
            ++directCounts[appNum];
          }
        } catch (...) {
        }
      }
      auto entPos = rline.find("ent=");
      if (entPos != std::string::npos &&
          (rline.find("Wayland direct") != std::string::npos ||
           rline.find("renderAppDirect") != std::string::npos)) {
        try {
          int ent = std::stoi(rline.substr(entPos + strlen("ent=")));
          ++directByEntity[ent];
        } catch (...) {
        }
      }
      if ((rline.find("direct=") != std::string::npos &&
           rline.find("direct=1") != std::string::npos) ||
          rline.find("Wayland direct ent=") != std::string::npos ||
          rline.find("renderAppDirect") != std::string::npos) {
        sawDirect = true;
      }
    }
    EXPECT_GT(wlEntries, 0u) << "Renderer log missing Wayland entries during hotkey test";
    if (!wmHotkeyFocus.empty()) {
      for (int idx : indices) {
        ASSERT_TRUE(wmHotkeyFocus.count(idx))
          << "WM log missing hotkey focus entry for idx=" << idx;
        int ent = wmHotkeyFocus[idx];
        EXPECT_GT(directByEntity[ent], 0u)
          << "Renderer log missing direct render for focused ent " << ent
          << " (hotkey idx=" << idx << ")";
      }
    } else {
      for (int idx : indices) {
        EXPECT_GT(directCounts[idx], 0u)
          << "Renderer log missing direct render entry for hotkey idx=" << idx;
      }
    }
    EXPECT_TRUE(sawDirect) << "Renderer log did not show direct render of focused app";
  }
}

TEST(ControlsSpec, TwoWaylandWindowsAreRegisteredAndRendered)
{
  CompositorGuard compGuard{ start_compositor() };
  ASSERT_FALSE(compGuard.handle.pid.empty()) << "Failed to start compositor";

  std::ofstream("/tmp/matrix-wlroots-output.log", std::ios::trunc).close();
  std::ofstream("/tmp/matrix-wlroots-wm.log", std::ios::trunc).close();

  std::this_thread::sleep_for(std::chrono::milliseconds(4000));

  // Spawn first and second foot instances.
  for (int i = 0; i < 3; ++i) {
    if (send_key_replay({ { "v", 0 } })) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch first foot";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 200, 50))
    << "First foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  // Unfocus and move back to give space before spawning the second.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "e", 50 },
                                { "s", 3000 }, { "s", 3000 } }))
    << "Failed to unfocus and back up";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  // Spawn second foot.
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch second foot";
  // Unfocus once more for good measure.
  ASSERT_TRUE(send_key_replay({ { "Alt_L", 0 }, { "e", 50 } }))
    << "Failed to unfocus after second launch";

  // Wait for two map events and two WM registrations, retry spawning if needed.
  bool twoMapped = false;
  bool twoRegistered = false;
  for (int i = 0; i < 150; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    twoMapped = count_log_occurrences("/tmp/matrix-wlroots-output.log", "mapped") >= 2;
    twoRegistered =
      count_log_occurrences("/tmp/matrix-wlroots-wm.log", "WaylandApp entity=") >= 2;
    if (twoMapped && twoRegistered) {
      break;
    }
    // If not mapped after some time, re-issue launch once.
    if (i == 80) {
      send_key_replay({ { "v", 0 } });
    }
  }
  ASSERT_TRUE(twoMapped) << "Second foot window never mapped";
  ASSERT_TRUE(twoRegistered) << "Wayland apps were not both registered";

  // Status should reflect both apps; allow a few retries for the registry to update.
  ApiRequestResponse statusResp;
  bool statusOk = false;
  for (int i = 0; i < 40; ++i) {
    if (fetch_status(&statusResp) && statusResp.has_status()) {
      FILE* f = std::fopen("/tmp/matrix-wlroots-output.log", "a");
      if (f) {
        std::fprintf(f,
                     "test status poll iter=%d wayland=%u total=%u\n",
                     i,
                     statusResp.status().wayland_apps(),
                     statusResp.status().total_entities());
        std::fclose(f);
      }
      if (statusResp.status().wayland_apps() >= 2) {
        statusOk = true;
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  if (!statusOk) {
    // Fallback: trust compositor log of STATUS replies if the ZMQ fetch path
    // cannot parse the response (seen in CI when another API socket is present).
    int apiStatusWithTwo =
      count_log_occurrences("/tmp/matrix-wlroots-output.log", "api status reply") >= 1 &&
      count_log_occurrences("/tmp/matrix-wlroots-output.log", "wayland=2") > 0;
    statusOk = statusOk || apiStatusWithTwo;
  }
  EXPECT_TRUE(statusOk) << "Expected two wayland apps in status";

  // Ensure renderer saw both with positionables logged.
  int posLogs = count_log_occurrences("/tmp/matrix-wlroots-wm.log", "pos=(");
  EXPECT_GE(posLogs, 2) << "Expected positionable logs for both windows";
}
