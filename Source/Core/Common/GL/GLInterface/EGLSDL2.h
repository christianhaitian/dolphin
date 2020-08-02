// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <string>
#include <vector>

#include "Common/GL/GLContext.h"

class GLContextEGLSDL2 : public GLContext
{
public:
  virtual ~GLContextEGLSDL2() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void UpdateSurface(void* window_handle) override;

  void Swap() override;
  void SwapInterval(int interval) override;

  void* GetFuncAddress(const std::string& name) override;

protected:
  bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override;

  void DetectMode();
  void DestroyContext();

  SDL_Window* m_window = nullptr;
  SDL_GLContext m_egl_context;
};
