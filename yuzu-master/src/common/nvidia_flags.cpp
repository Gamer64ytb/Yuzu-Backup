// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>

#include <fmt/format.h>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/nvidia_flags.h"

namespace Common {

void ConfigureNvidiaEnvironmentFlags() {
#ifdef _WIN32
    const auto nvidia_shader_dir =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::ShaderDir) / "nvidia";

    if (!Common::FS::CreateDirs(nvidia_shader_dir)) {
        return;
    }

    const auto windows_path_string =
        Common::FS::PathToUTF8String(nvidia_shader_dir.lexically_normal());

    void(_putenv(fmt::format("__GL_SHADER_DISK_CACHE_PATH={}", windows_path_string).c_str()));
    void(_putenv("__GL_SHADER_DISK_CACHE_SKIP_CLEANUP=1"));
    void(_putenv("__GL_THREADED_OPTIMIZATIONS=1"));
#endif
}

} // namespace Common
