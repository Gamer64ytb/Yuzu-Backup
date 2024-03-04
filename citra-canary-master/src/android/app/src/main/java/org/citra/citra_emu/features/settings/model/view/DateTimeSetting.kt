// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.model.view

import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.AbstractStringSetting

class DateTimeSetting(
    setting: AbstractSetting?,
    titleId: Int,
    descriptionId: Int,
    val key: String? = null,
    private val defaultValue: String? = null
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_DATETIME_SETTING

    val value: String
        get() = if (setting != null) {
            val setting = setting as AbstractStringSetting
            setting.string
        } else {
            defaultValue!!
        }

    fun setSelectedValue(datetime: String): AbstractStringSetting {
        val stringSetting = setting as AbstractStringSetting
        stringSetting.string = datetime
        return stringSetting
    }
}
