// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.features.settings.model.view.InputProfileSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.utils.ViewUtils.setVisible

class InputProfileViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: InputProfileSetting

    override fun bind(item: SettingsItem) {
        setting = item as InputProfileSetting
        binding.textSettingName.text = setting.title
        binding.textSettingValue.text =
            setting.getCurrentProfile().ifEmpty { binding.root.context.getString(R.string.not_set) }

        binding.textSettingDescription.setVisible(false)
        binding.buttonClear.setVisible(false)
        binding.icon.setVisible(false)
        binding.buttonClear.setVisible(false)
    }

    override fun onClick(clicked: View) =
        adapter.onInputProfileClick(setting, bindingAdapterPosition)

    override fun onLongClick(clicked: View): Boolean = false
}
