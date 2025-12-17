#pragma once

#include <glad/glad.h>
#include <utility>

// Simple RAII wrappers for GL objects. These own a single name and are
// move-only to prevent accidental double-deletes. They intentionally do not
// hide the underlying GLuint so existing call sites can bind directly.

class GlBuffer
{
public:
  GlBuffer() = default;
  explicit GlBuffer(GLenum target) { create(target); }
  ~GlBuffer() { reset(); }

  GlBuffer(const GlBuffer&) = delete;
  GlBuffer& operator=(const GlBuffer&) = delete;

  GlBuffer(GlBuffer&& other) noexcept { swap(other); }
  GlBuffer& operator=(GlBuffer&& other) noexcept
  {
    if (this != &other) {
      reset();
      swap(other);
    }
    return *this;
  }

  void create(GLenum newTarget)
  {
    reset();
    target = newTarget;
    glGenBuffers(1, &id);
  }

  void reset()
  {
    if (id != 0) {
      glDeleteBuffers(1, &id);
      id = 0;
    }
    target = 0;
  }

  GLuint get() const { return id; }
  GLenum getTarget() const { return target; }
  void bind() const { glBindBuffer(target, id); }
  explicit operator bool() const { return id != 0; }
  operator GLuint() const { return id; }

private:
  void swap(GlBuffer& other) noexcept
  {
    std::swap(id, other.id);
    std::swap(target, other.target);
  }

  GLuint id = 0;
  GLenum target = 0;
};

class GlVertexArray
{
public:
  GlVertexArray() = default;
  explicit GlVertexArray(bool createNow) { if (createNow) create(); }
  ~GlVertexArray() { reset(); }

  GlVertexArray(const GlVertexArray&) = delete;
  GlVertexArray& operator=(const GlVertexArray&) = delete;

  GlVertexArray(GlVertexArray&& other) noexcept { swap(other); }
  GlVertexArray& operator=(GlVertexArray&& other) noexcept
  {
    if (this != &other) {
      reset();
      swap(other);
    }
    return *this;
  }

  void create()
  {
    reset();
    glGenVertexArrays(1, &id);
  }

  void reset()
  {
    if (id != 0) {
      glDeleteVertexArrays(1, &id);
      id = 0;
    }
  }

  GLuint get() const { return id; }
  void bind() const { glBindVertexArray(id); }
  explicit operator bool() const { return id != 0; }
  operator GLuint() const { return id; }

private:
  void swap(GlVertexArray& other) noexcept { std::swap(id, other.id); }

  GLuint id = 0;
};
