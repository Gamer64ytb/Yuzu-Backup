// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include "core/file_sys/vfs/vfs_types.h"
#include "core/hle/result.h"

namespace FileSys {

class RegisteredCache;
class PlaceholderCache;

/// File system interface to the SDCard archive
class SDMCFactory {
public:
    explicit SDMCFactory(VirtualDir sd_dir_, VirtualDir sd_mod_dir_);
    ~SDMCFactory();

    VirtualDir Open() const;

    VirtualDir GetSDMCModificationLoadRoot(u64 title_id) const;
    VirtualDir GetSDMCContentDirectory() const;

    RegisteredCache* GetSDMCContents() const;
    PlaceholderCache* GetSDMCPlaceholder() const;

    VirtualDir GetImageDirectory() const;

    u64 GetSDMCFreeSpace() const;
    u64 GetSDMCTotalSpace() const;

private:
    VirtualDir sd_dir;
    VirtualDir sd_mod_dir;

    std::unique_ptr<RegisteredCache> contents;
    std::unique_ptr<PlaceholderCache> placeholder;
};

} // namespace FileSys
