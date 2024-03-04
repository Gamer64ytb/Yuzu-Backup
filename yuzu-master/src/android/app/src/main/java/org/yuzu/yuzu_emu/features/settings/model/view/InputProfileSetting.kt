// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.input.NativeInput
import org.yuzu.yuzu_emu.utils.NativeConfig

class InputProfileSetting(private val playerIndex: Int) :
    SettingsItem(emptySetting, R.string.profile, "", 0, "") {
    override val type = TYPE_INPUT_PROFILE

    fun getCurrentProfile(): String =
        NativeConfig.getInputSettings(true)[playerIndex].profileName

    fun getProfileNames(): Array<String> = NativeInput.getInputProfileNames()

    fun isProfileNameValid(name: String): Boolean = NativeInput.isProfileNameValid(name)

    fun createProfile(name: String): Boolean = NativeInput.createProfile(name, playerIndex)

    fun deleteProfile(name: String): Boolean = NativeInput.deleteProfile(name, playerIndex)

    fun loadProfile(name: String): Boolean {
        val result = NativeInput.loadProfile(name, playerIndex)
        NativeInput.reloadInputDevices()
        return result
    }

    fun saveProfile(name: String): Boolean = NativeInput.saveProfile(name, playerIndex)
}
