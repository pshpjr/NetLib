#pragma once

#ifdef _WIN32
#include "WindowsExecutor.h"
using Executor  = WindowsExecutor;
#else
#include "LinuxExecutor.h"
using Executor = LinuxExecutor;
#endif