// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32
#include <windows.h>
#elif defined(YUZU_UNIX)
#include <sys/types.h>
#endif

constexpr char IS_CHILD_ENV_VAR[] = "YUZU_IS_CHILD";
constexpr char STARTUP_CHECK_ENV_VAR[] = "YUZU_DO_STARTUP_CHECKS";
constexpr char ENV_VAR_ENABLED_TEXT[] = "ON";

void CheckVulkan();
bool CheckEnvVars(bool* is_child);
bool StartupChecks(const char* arg0, bool* has_broken_vulkan, bool perform_vulkan_check);

#ifdef _WIN32
bool SpawnChild(const char* arg0, PROCESS_INFORMATION* pi, int flags);
#elif defined(YUZU_UNIX)
pid_t SpawnChild(const char* arg0);
#endif
