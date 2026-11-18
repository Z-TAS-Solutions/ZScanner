#pragma once

#if defined(_WIN32)
  #include "platform/windows/WinForge.hpp"
  using Window = WinForge;
#elif defined(__linux__)
  #include "platform/linux/LinuxForge.hpp"
  using Window = LinuxForge;
#endif
