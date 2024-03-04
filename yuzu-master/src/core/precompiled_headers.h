// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <boost/container/flat_map.hpp> // used by service.h which is heavily included
#include <boost/intrusive/rbtree.hpp>   // used by k_auto_object.h which is heavily included

#include "common/common_precompiled_headers.h"

#include "core/hle/kernel/k_process.h"
