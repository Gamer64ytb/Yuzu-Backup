// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

interface AbstractLongSetting : AbstractSetting {
    fun getLong(needsGlobal: Boolean = false): Long
    fun setLong(value: Long)
}
