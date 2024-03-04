// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.model

interface AbstractSetting {
    val key: String?
    val section: String?
    val isRuntimeEditable: Boolean
    val valueAsString: String
    val defaultValue: Any
}
