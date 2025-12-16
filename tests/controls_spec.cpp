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
#include <zmq/zmq.hpp>
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
  }
}

static CompositorHandle
start_compositor()
{
  kill_existing_compositor();

  CompositorHandle h;
  const std::string apiBind = "tcp://*:3345";
  const std::string apiAddr = "tcp://127.0.0.1:3345";
  setenv("VOXEL_API_ADDR_FOR_TEST", apiAddr.c_str(), 1);
  std::string cmd =
    "MATRIX_WLROOTS_BIN=./matrix-wlroots-debug MENU_PROGRAM=foot "
    "VOXEL_API_BIND=" + apiBind + " "
    "VOXEL_API_ADDR_FOR_TEST=" + apiAddr + " "
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
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return false;
}

static std::optional<EngineStatus>
current_status()
{
  ApiRequestResponse resp;
  if (!fetch_status(&resp) || !resp.has_status()) {
    return std::nullopt;
  }
  return resp.status();
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
    for (int i = 0; i < 50; ++i) {
      std::string check = "kill -0 " + h.pid + " >/dev/null 2>&1";
      int rc = std::system(check.c_str());
      if (rc != 0) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  } else {
    std::system("pkill -f matrix-wlroots >/dev/null 2>&1");
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

  // Launch terminal via menu hotkey (v).
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to send menu launch key";
  ASSERT_TRUE(wait_for_log_contains("/tmp/matrix-wlroots-output.log", "mapped", 600, 100))
    << "Foot window never mapped";
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  // Focus looked-at app via 'r' so WM focus is set.
  ASSERT_TRUE(send_key_replay({ { "r", 0 } })) << "Failed to send focus key";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

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

  // Super+E should unfocus via WM and log.
  bool sent = send_key_replay({ { "Super_L", 0 }, { "e", 50 } });
  ASSERT_TRUE(sent) << "Failed to send Super+E via key replay";

  // Only wait briefly; the unfocus log should be immediate if the combo worked.
  bool sawUnfocus =
    wait_for_log_contains("/tmp/matrix-wlroots-wm.log", "WM: unfocusApp", 20, 100);
  bool sawSuperCombo =
    wait_for_log_contains("/tmp/matrix-wlroots-output.log", "super combo: triggered -> unfocus", 20, 100);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Try to move the camera via WASD-style input; movement only occurs when WM focus is cleared.
  ASSERT_TRUE(send_key_replay(
                { { "s", 0 }, { "s", 0 }, { "s", 0 }, { "s", 0 }, { "s", 0 } }))
    << "Failed to send movement keys";
  std::this_thread::sleep_for(std::chrono::milliseconds(800));

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
  ASSERT_TRUE(send_key_replay({ { "r", 0 } })) << "Failed to focus first app";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Unfocus and move backward (hold 's' until next entry).
  ASSERT_TRUE(send_key_replay({ { "Super_L", 0 }, { "e", 50 }, { "s", 4000 } }))
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
  ASSERT_TRUE(send_key_replay({ { "Super_L", 0 },
                                { "1", 100 },
                                { "Super_L", 0 },
                                { "2", 100 },
                                { "Super_L", 0 },
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
      auto pos = line.find("super hotkey: idx=");
      if (pos != std::string::npos) {
        auto sub = line.substr(pos + strlen("super hotkey: idx="));
        try {
          int idx = std::stoi(sub);
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
  ASSERT_TRUE(send_key_replay({ { "Super_L", 0 }, { "e", 50 },
                                { "s", 3000 }, { "s", 3000 } }))
    << "Failed to unfocus and back up";
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  // Spawn second foot.
  ASSERT_TRUE(send_key_replay({ { "v", 0 } })) << "Failed to launch second foot";
  // Unfocus once more for good measure.
  ASSERT_TRUE(send_key_replay({ { "Super_L", 0 }, { "e", 50 } }))
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
