#pragma once

#ifdef _WIN32
#include "WindowsCloseSignalListener.h"
#else
#include "LinuxCloseSignalListener.h"
#endif

namespace psh::util
{
#ifdef _WIN32
    using CloseSignalListener = WindowsCloseSignalListener;
#else
    using CloseSignalListener = LinuxCloseSignalListener;
#endif
}
