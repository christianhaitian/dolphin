// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "Common/GL/GLInterface/EGLSDL2.h"
#include "Common/Logging/Log.h"

GLContextEGLSDL2::~GLContextEGLSDL2()
{
  DestroyContext();
}

bool GLContextEGLSDL2::IsHeadless() const
{
  return m_wsi.type == WindowSystemType::Headless;
}

void GLContextEGLSDL2::Swap()
{
  if (m_window != nullptr)
    SDL_GL_SwapWindow(m_window);
}

void GLContextEGLSDL2::SwapInterval(int interval)
{
  SDL_GL_SetSwapInterval(interval);
}

void* GLContextEGLSDL2::GetFuncAddress(const std::string& name)
{
  return (void*)SDL_GL_GetProcAddress(name.c_str());
}

void GLContextEGLSDL2::DetectMode()
{
  bool supportsGLCore = false, supportsGLCompatibility = false, supportsGLES3 = false;

  // Setup GL context requirements similar to EGL driver
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

  // Test for OpenGL ES 3.0
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GLContext gles3_context = SDL_GL_CreateContext(m_window);
  if (gles3_context != nullptr)
  {
    supportsGLES3 = true;
    SDL_GL_DeleteContext(gles3_context);
  }
  else
    INFO_LOG(VIDEO, "Error: couldn't get a GLES 3.0 context through SDL2");


  // Test for OpenGL 3.0 (core profile)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GLContext glcore3_context = SDL_GL_CreateContext(m_window);
  if (glcore3_context != nullptr)
  {
    supportsGLCore = true;
    SDL_GL_DeleteContext(glcore3_context);
  }
  else
    INFO_LOG(VIDEO, "Error: couldn't get a GL Core 3.0 context through SDL2");

  // Test for OpenGL 3.0 (compatibility profile)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GLContext glcomp3_context = SDL_GL_CreateContext(m_window);
  if (glcomp3_context != nullptr)
  {
    supportsGLCompatibility = true;
    SDL_GL_DeleteContext(glcomp3_context);
  }
  else
    INFO_LOG(VIDEO, "Error: couldn't get a GL Compatibility 3.0 context through SDL2");


  if (supportsGLCore)
  {
    INFO_LOG(VIDEO, "Using OpenGL (Core)");
    m_opengl_mode = Mode::OpenGL;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  }
  else if (supportsGLCompatibility)
  {
    INFO_LOG(VIDEO, "Using OpenGL (Compatibility)");
    m_opengl_mode = Mode::OpenGL;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  }
  else if (supportsGLES3)
  {
    INFO_LOG(VIDEO, "Using OpenGL|ES");
    m_opengl_mode = Mode::OpenGLES;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  }
  else
  {
    // Errored before we found a mode
    ERROR_LOG(VIDEO, "Error: Failed to detect OpenGL flavour, falling back to OpenGL (Compatibility)");
    // This will fail to create a context, as it'll try to use the same attribs we just failed to
    // find a matching config with
    m_opengl_mode = Mode::OpenGL;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  }
}

SDL_Window* GLContextEGLSDL2::OpenSDL2Window()
{
  // Retrieve physical width and height from current display mode
  SDL_DisplayMode dm;
  if (SDL_GetDesktopDisplayMode(0, &dm) != 0)
  {
    ERROR_LOG(VIDEO, "SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
    return nullptr;
  }

  // Create OpenGL-enabled fullscreen window
  return SDL_CreateWindow("Dolphin", 0, 0, dm.w, dm.h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool GLContextEGLSDL2::Initialize(const WindowSystemInfo& wsi, bool stereo, bool core)
{
  m_wsi = wsi;

  // Init only video subsystem
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    ERROR_LOG(VIDEO, "SDL_Init failed %s", SDL_GetError());
  }

  m_window = OpenSDL2Window();
  if (!m_window)
  {
    INFO_LOG(VIDEO, "Error: SDL_CreateWindow() failed");
    return false;
  }

  // TODO handle hardcoded modes
  // if (m_opengl_mode == Mode::Detect)
  DetectMode();

  m_egl_context = SDL_GL_CreateContext(m_window);
  if (!m_egl_context)
  {
    ERROR_LOG(VIDEO,"Error: SDL_GL_CreateContext failed, %s", SDL_GetError());
    return false;
  }

  return MakeCurrent();
}

std::unique_ptr<GLContext> GLContextEGLSDL2::CreateSharedContext()
{
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  SDL_GLContext new_egl_context = SDL_GL_CreateContext(m_window);
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
  if (!new_egl_context)
  {
    INFO_LOG(VIDEO, "Error: SDL_GL_CreateContext (shared) failed: %s", SDL_GetError());
    return nullptr;
  }

  std::unique_ptr<GLContextEGLSDL2> new_context = std::make_unique<GLContextEGLSDL2>();
  new_context->m_opengl_mode = m_opengl_mode;
  new_context->m_egl_context = new_egl_context;
  new_context->m_wsi.display_connection = m_wsi.display_connection;
  new_context->m_window = m_window;
  new_context->m_supports_surfaceless = m_supports_surfaceless;
  new_context->m_is_shared = true;

  return new_context;
}

bool GLContextEGLSDL2::MakeCurrent()
{
  return SDL_GL_MakeCurrent(m_window, m_egl_context) == 0;
}

void GLContextEGLSDL2::UpdateSurface(void* window_handle)
{
  m_wsi.render_surface = window_handle;
  ClearCurrent();
  MakeCurrent();
}

bool GLContextEGLSDL2::ClearCurrent()
{
  return SDL_GL_MakeCurrent(m_window, nullptr) == 0;
}

// Close backend
void GLContextEGLSDL2::DestroyContext()
{
  if (!m_egl_context)
    return;
  SDL_GL_DeleteContext(m_egl_context);
  m_egl_context = nullptr;
  SDL_DestroyWindow(m_window);
  m_window = nullptr;
}
