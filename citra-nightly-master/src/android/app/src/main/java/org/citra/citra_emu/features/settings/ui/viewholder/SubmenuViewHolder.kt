// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.ui.viewholder

import android.view.View
import org.citra.citra_emu.databinding.ListItemSettingBinding
import org.citra.citra_emu.features.settings.model.view.SettingsItem
import org.citra.citra_emu.features.settings.model.view.SubmenuSetting
import org.citra.citra_emu.features.settings.ui.SettingsAdapter

class SubmenuViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var item: SubmenuSetting

    override fun bind(item: SettingsItem) {
        this.item = item as SubmenuSetting
        binding.textSettingName.setText(item.nameId)
        if (item.descriptionId != 0) {
            binding.textSettingDescription.setText(item.descriptionId)
            binding.textSettingDescription.visibility = View.VISIBLE
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
    }

    override fun onClick(clicked: View) {
        adapter.onSubmenuClick(item)
    }

    override fun onLongClick(clicked: View): Boolean {
        // no-op
        return true
    }
}
