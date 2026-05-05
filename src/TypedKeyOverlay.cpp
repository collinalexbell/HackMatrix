#include "TypedKeyOverlay.h"

#include "Config.h"
#include "shader.h"
#include "time_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using GlyphRows = std::array<const char*, 7>;

const GlyphRows&
glyph_rows(char ch)
{
  static const std::unordered_map<char, GlyphRows> kGlyphs = {
    {'A', {"01110", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'B', {"11110", "10001", "10001", "11110", "10001", "10001", "11110"}},
    {'C', {"01111", "10000", "10000", "10000", "10000", "10000", "01111"}},
    {'D', {"11110", "10001", "10001", "10001", "10001", "10001", "11110"}},
    {'E', {"11111", "10000", "10000", "11110", "10000", "10000", "11111"}},
    {'F', {"11111", "10000", "10000", "11110", "10000", "10000", "10000"}},
    {'G', {"01111", "10000", "10000", "10111", "10001", "10001", "01111"}},
    {'H', {"10001", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'I', {"11111", "00100", "00100", "00100", "00100", "00100", "11111"}},
    {'J', {"00111", "00010", "00010", "00010", "10010", "10010", "01100"}},
    {'K', {"10001", "10010", "10100", "11000", "10100", "10010", "10001"}},
    {'L', {"10000", "10000", "10000", "10000", "10000", "10000", "11111"}},
    {'M', {"10001", "11011", "10101", "10101", "10001", "10001", "10001"}},
    {'N', {"10001", "10001", "11001", "10101", "10011", "10001", "10001"}},
    {'O', {"01110", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'P', {"11110", "10001", "10001", "11110", "10000", "10000", "10000"}},
    {'Q', {"01110", "10001", "10001", "10001", "10101", "10010", "01101"}},
    {'R', {"11110", "10001", "10001", "11110", "10100", "10010", "10001"}},
    {'S', {"01111", "10000", "10000", "01110", "00001", "00001", "11110"}},
    {'T', {"11111", "00100", "00100", "00100", "00100", "00100", "00100"}},
    {'U', {"10001", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'V', {"10001", "10001", "10001", "10001", "10001", "01010", "00100"}},
    {'W', {"10001", "10001", "10001", "10101", "10101", "10101", "01010"}},
    {'X', {"10001", "10001", "01010", "00100", "01010", "10001", "10001"}},
    {'Y', {"10001", "10001", "01010", "00100", "00100", "00100", "00100"}},
    {'Z', {"11111", "00001", "00010", "00100", "01000", "10000", "11111"}},
    {'0', {"01110", "10001", "10011", "10101", "11001", "10001", "01110"}},
    {'1', {"00100", "01100", "00100", "00100", "00100", "00100", "01110"}},
    {'2', {"01110", "10001", "00001", "00010", "00100", "01000", "11111"}},
    {'3', {"11110", "00001", "00001", "01110", "00001", "00001", "11110"}},
    {'4', {"00010", "00110", "01010", "10010", "11111", "00010", "00010"}},
    {'5', {"11111", "10000", "10000", "11110", "00001", "00001", "11110"}},
    {'6', {"01110", "10000", "10000", "11110", "10001", "10001", "01110"}},
    {'7', {"11111", "00001", "00010", "00100", "01000", "01000", "01000"}},
    {'8', {"01110", "10001", "10001", "01110", "10001", "10001", "01110"}},
    {'9', {"01110", "10001", "10001", "01111", "00001", "00001", "01110"}},
    {'+', {"00000", "00100", "00100", "11111", "00100", "00100", "00000"}},
    {'-', {"00000", "00000", "00000", "11111", "00000", "00000", "00000"}},
    {'.', {"00000", "00000", "00000", "00000", "00000", "01100", "01100"}},
    {',', {"00000", "00000", "00000", "00000", "00110", "00110", "00100"}},
    {'/', {"00001", "00010", "00100", "01000", "10000", "00000", "00000"}},
    {'\\', {"10000", "01000", "00100", "00010", "00001", "00000", "00000"}},
    {'=', {"00000", "11111", "00000", "11111", "00000", "00000", "00000"}},
    {';', {"00000", "01100", "01100", "00000", "01100", "01100", "00100"}},
    {':', {"00000", "01100", "01100", "00000", "01100", "01100", "00000"}},
    {'[', {"01110", "01000", "01000", "01000", "01000", "01000", "01110"}},
    {']', {"01110", "00010", "00010", "00010", "00010", "00010", "01110"}},
    {'\'', {"00100", "00100", "00010", "00000", "00000", "00000", "00000"}},
    {'"', {"01010", "01010", "00100", "00000", "00000", "00000", "00000"}},
    {'?', {"01110", "10001", "00001", "00010", "00100", "00000", "00100"}},
    {'!', {"00100", "00100", "00100", "00100", "00100", "00000", "00100"}},
    {' ', {"00000", "00000", "00000", "00000", "00000", "00000", "00000"}},
  };
  static const GlyphRows kUnknown = {
    "11111", "10001", "00010", "00100", "00100", "00000", "00100"
  };

  auto it = kGlyphs.find(ch);
  return it != kGlyphs.end() ? it->second : kUnknown;
}

std::vector<unsigned char>
rasterize_overlay_text(const std::string& text, int& outWidth, int& outHeight)
{
  constexpr int glyphWidth = 5;
  constexpr int glyphHeight = 7;
  const int scale = std::max(4, static_cast<int>(std::lround(48.0 / glyphHeight)));
  const int glyphAdvance = (glyphWidth + 1) * scale;
  const int verticalPadding = scale;
  outWidth = std::max(1, static_cast<int>(text.size()) * glyphAdvance);
  outHeight = glyphHeight * scale + verticalPadding * 2;

  std::vector<unsigned char> pixels(
    static_cast<size_t>(outWidth * outHeight * 4), 0);

  for (size_t i = 0; i < text.size(); ++i) {
    const GlyphRows& rows = glyph_rows(text[i]);
    const int baseX = static_cast<int>(i) * glyphAdvance;
    for (int gy = 0; gy < glyphHeight; ++gy) {
      for (int gx = 0; gx < glyphWidth; ++gx) {
        if (rows[gy][gx] != '1') {
          continue;
        }
        for (int sy = 0; sy < scale; ++sy) {
          for (int sx = 0; sx < scale; ++sx) {
            const int px = baseX + gx * scale + sx;
            const int py = verticalPadding + gy * scale + sy;
            if (px < 0 || py < 0 || px >= outWidth || py >= outHeight) {
              continue;
            }
            size_t idx =
              static_cast<size_t>(((outHeight - 1 - py) * outWidth + px) * 4);
            pixels[idx + 0] = 255;
            pixels[idx + 1] = 255;
            pixels[idx + 2] = 255;
            pixels[idx + 3] = 230;
          }
        }
      }
    }
  }

  return pixels;
}

std::string
display_token_for_keysym(xkb_keysym_t sym)
{
  switch (sym) {
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
      return "ENTER";
    case XKB_KEY_Tab:
      return "TAB";
    case XKB_KEY_BackSpace:
      return "BKSP";
    case XKB_KEY_Delete:
      return "DEL";
    case XKB_KEY_Escape:
      return "ESC";
    case XKB_KEY_space:
      return "SPACE";
    case XKB_KEY_Left:
      return "LEFT";
    case XKB_KEY_Right:
      return "RIGHT";
    case XKB_KEY_Up:
      return "UP";
    case XKB_KEY_Down:
      return "DOWN";
    case XKB_KEY_Shift_L:
    case XKB_KEY_Shift_R:
      return "SHIFT";
    case XKB_KEY_Control_L:
    case XKB_KEY_Control_R:
      return "CTRL";
    case XKB_KEY_Alt_L:
    case XKB_KEY_Alt_R:
      return "ALT";
    case XKB_KEY_Super_L:
    case XKB_KEY_Super_R:
    case XKB_KEY_Meta_L:
    case XKB_KEY_Meta_R:
      return "SUPER";
    default:
      break;
  }

  uint32_t codepoint = xkb_keysym_to_utf32(sym);
  if (codepoint >= 33 && codepoint <= 126) {
    char ch = static_cast<char>(codepoint);
    if (std::isalpha(static_cast<unsigned char>(ch))) {
      ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return std::string(1, ch);
  }

  char name[64] = {};
  int len = xkb_keysym_get_name(sym, name, sizeof(name));
  if (len <= 0) {
    return "";
  }

  std::string token(name, static_cast<size_t>(len));
  for (char& ch : token) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return token;
}

std::string
join_tokens(const std::vector<std::string>& tokens)
{
  std::string text;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (!text.empty()) {
      text += ' ';
    }
    text += tokens[i];
  }
  return text;
}

bool
show_key_press_overlay_enabled()
{
  try {
    return Config::singleton()->get<bool>("show_key_press_overlay");
  } catch (...) {
    return false;
  }
}

} // namespace

TypedKeyOverlay::~TypedKeyOverlay()
{
  if (texture != 0) {
    glDeleteTextures(1, &texture);
  }
}

void
TypedKeyOverlay::recordKeysym(xkb_keysym_t sym)
{
  if (!show_key_press_overlay_enabled()) {
    return;
  }

  std::string token = display_token_for_keysym(sym);
  if (token.empty()) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex);
  tokens.push_back(std::move(token));
  expiresAt = nowSeconds() + 2.0;

  size_t totalChars = 0;
  for (const auto& item : tokens) {
    totalChars += item.size() + 1;
  }
  while (tokens.size() > 12 || totalChars > 48) {
    totalChars -= tokens.front().size() + 1;
    tokens.erase(tokens.begin());
  }
}

void
TypedKeyOverlay::render(Shader* shader,
                        GLuint vao,
                        GLuint vbo,
                        float screenWidth,
                        float screenHeight,
                        bool appFocused)
{
  if (shader == nullptr || appFocused || !show_key_press_overlay_enabled()) {
    return;
  }

  std::string text;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (tokens.empty() || nowSeconds() > expiresAt) {
      return;
    }
    text = join_tokens(tokens);
  }

  int texWidth = 0;
  int texHeight = 0;
  std::vector<unsigned char> pixels = rasterize_overlay_text(text, texWidth, texHeight);
  if (pixels.empty() || texWidth <= 0 || texHeight <= 0) {
    return;
  }

  if (texture == 0) {
    glGenTextures(1, &texture);
  }

  shader->use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               texWidth,
               texHeight,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               pixels.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  float overlayWidth = static_cast<float>(texWidth);
  float overlayHeight = static_cast<float>(texHeight);
  float originX = (screenWidth - overlayWidth) * 0.5f;
  float originY = 18.0f;
  float yFlipped = screenHeight - originY - overlayHeight;
  float left = originX / screenWidth * 2.0f - 1.0f;
  float right = (originX + overlayWidth) / screenWidth * 2.0f - 1.0f;
  float top = 1.0f - yFlipped / screenHeight * 2.0f;
  float bottom = 1.0f - (yFlipped + overlayHeight) / screenHeight * 2.0f;

  float verts[] = {
    left,  bottom, 0.0f, 0.0f, 1.0f, right, bottom, 0.0f, 1.0f, 1.0f,
    right, top,    0.0f, 1.0f, 0.0f, left,  top,    0.0f, 0.0f, 0.0f,
    left,  bottom, 0.0f, 0.0f, 1.0f, right, top,    0.0f, 1.0f, 0.0f
  };

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  glDisable(GL_DEPTH_TEST);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  if (depthEnabled) {
    glEnable(GL_DEPTH_TEST);
  }
}
