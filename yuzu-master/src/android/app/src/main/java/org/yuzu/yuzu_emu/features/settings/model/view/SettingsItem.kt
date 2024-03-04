// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.annotation.StringRes
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.input.NativeInput
import org.yuzu.yuzu_emu.features.input.model.NpadStyleIndex
import org.yuzu.yuzu_emu.features.settings.model.AbstractBooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.ByteSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.LongSetting
import org.yuzu.yuzu_emu.features.settings.model.ShortSetting
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Each one corresponds to a [AbstractSetting] object, so this class's subclasses
 * should vaguely correspond to those subclasses. There are a few with multiple analogues
 * and a few with none (Headers, for example, do not correspond to anything in the ini
 * file.)
 */
abstract class SettingsItem(
    val setting: AbstractSetting,
    @StringRes val titleId: Int,
    val titleString: String,
    @StringRes val descriptionId: Int,
    val descriptionString: String
) {
    abstract val type: Int

    val title: String by lazy {
        if (titleId != 0) {
            return@lazy YuzuApplication.appContext.getString(titleId)
        }
        return@lazy titleString
    }

    val description: String by lazy {
        if (descriptionId != 0) {
            return@lazy YuzuApplication.appContext.getString(descriptionId)
        }
        return@lazy descriptionString
    }

    val isEditable: Boolean
        get() {
            // Can't change docked mode toggle when using handheld mode
            if (setting.key == BooleanSetting.USE_DOCKED_MODE.key) {
                return NativeInput.getStyleIndex(0) != NpadStyleIndex.Handheld
            }

            // Can't edit settings that aren't saveable in per-game config even if they are switchable
            if (NativeConfig.isPerGameConfigLoaded() && !setting.isSaveable) {
                return false
            }

            if (!NativeLibrary.isRunning()) return true

            // Prevent editing settings that were modified in per-game config while editing global
            // config
            if (!NativeConfig.isPerGameConfigLoaded() && !setting.global) {
                return false
            }

            return setting.isRuntimeModifiable
        }

    val needsRuntimeGlobal: Boolean
        get() = NativeLibrary.isRunning() && !setting.global &&
            !NativeConfig.isPerGameConfigLoaded()

    val clearable: Boolean
        get() = !setting.global && NativeConfig.isPerGameConfigLoaded()

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_SWITCH = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
        const val TYPE_STRING_SINGLE_CHOICE = 5
        const val TYPE_DATETIME_SETTING = 6
        const val TYPE_RUNNABLE = 7
        const val TYPE_INPUT = 8
        const val TYPE_INT_SINGLE_CHOICE = 9
        const val TYPE_INPUT_PROFILE = 10
        const val TYPE_STRING_INPUT = 11

        const val FASTMEM_COMBINED = "fastmem_combined"

        val emptySetting = object : AbstractSetting {
            override val key: String = ""
            override val defaultValue: Any = false
            override val isSaveable = true
            override fun getValueAsString(needsGlobal: Boolean): String = ""
            override fun reset() {}
        }

        // Extension for putting SettingsItems into a hashmap without repeating yourself
        fun HashMap<String, SettingsItem>.put(item: SettingsItem) {
            put(item.setting.key, item)
        }

        // List of all general
        val settingsItems = HashMap<String, SettingsItem>().apply {
            put(StringInputSetting(StringSetting.DEVICE_NAME, titleId = R.string.device_name))
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_SPEED_LIMIT,
                    titleId = R.string.frame_limit_enable,
                    descriptionId = R.string.frame_limit_enable_description
                )
            )
            put(
                SliderSetting(
                    ShortSetting.RENDERER_SPEED_LIMIT,
                    titleId = R.string.frame_limit_slider,
                    descriptionId = R.string.frame_limit_slider_description,
                    min = 1,
                    max = 400,
                    units = "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.CPU_BACKEND,
                    titleId = R.string.cpu_backend,
                    choicesId = R.array.cpuBackendArm64Names,
                    valuesId = R.array.cpuBackendArm64Values
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.CPU_ACCURACY,
                    titleId = R.string.cpu_accuracy,
                    choicesId = R.array.cpuAccuracyNames,
                    valuesId = R.array.cpuAccuracyValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.PICTURE_IN_PICTURE,
                    titleId = R.string.picture_in_picture,
                    descriptionId = R.string.picture_in_picture_description
                )
            )

            val dockedModeSetting = object : AbstractBooleanSetting {
                override val key = BooleanSetting.USE_DOCKED_MODE.key

                override fun getBoolean(needsGlobal: Boolean): Boolean {
                    if (NativeInput.getStyleIndex(0) == NpadStyleIndex.Handheld) {
                        return false
                    }
                    return BooleanSetting.USE_DOCKED_MODE.getBoolean(needsGlobal)
                }

                override fun setBoolean(value: Boolean) =
                    BooleanSetting.USE_DOCKED_MODE.setBoolean(value)

                override val defaultValue = BooleanSetting.USE_DOCKED_MODE.defaultValue

                override fun getValueAsString(needsGlobal: Boolean): String =
                    BooleanSetting.USE_DOCKED_MODE.getValueAsString(needsGlobal)

                override fun reset() = BooleanSetting.USE_DOCKED_MODE.reset()
            }
            put(
                SwitchSetting(
                    dockedModeSetting,
                    titleId = R.string.use_docked_mode,
                    descriptionId = R.string.use_docked_mode_description
                )
            )

            put(
                SingleChoiceSetting(
                    IntSetting.REGION_INDEX,
                    titleId = R.string.emulated_region,
                    choicesId = R.array.regionNames,
                    valuesId = R.array.regionValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.LANGUAGE_INDEX,
                    titleId = R.string.emulated_language,
                    choicesId = R.array.languageNames,
                    valuesId = R.array.languageValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.USE_CUSTOM_RTC,
                    titleId = R.string.use_custom_rtc,
                    descriptionId = R.string.use_custom_rtc_description
                )
            )
            put(DateTimeSetting(LongSetting.CUSTOM_RTC, titleId = R.string.set_custom_rtc))
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ACCURACY,
                    titleId = R.string.renderer_accuracy,
                    choicesId = R.array.rendererAccuracyNames,
                    valuesId = R.array.rendererAccuracyValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_RESOLUTION,
                    titleId = R.string.renderer_resolution,
                    choicesId = R.array.rendererResolutionNames,
                    valuesId = R.array.rendererResolutionValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_VSYNC,
                    titleId = R.string.renderer_vsync,
                    choicesId = R.array.rendererVSyncNames,
                    valuesId = R.array.rendererVSyncValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCALING_FILTER,
                    titleId = R.string.renderer_scaling_filter,
                    choicesId = R.array.rendererScalingFilterNames,
                    valuesId = R.array.rendererScalingFilterValues
                )
            )
            put(
                SliderSetting(
                    IntSetting.FSR_SHARPENING_SLIDER,
                    titleId = R.string.fsr_sharpness,
                    descriptionId = R.string.fsr_sharpness_description,
                    units = "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ANTI_ALIASING,
                    titleId = R.string.renderer_anti_aliasing,
                    choicesId = R.array.rendererAntiAliasingNames,
                    valuesId = R.array.rendererAntiAliasingValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCREEN_LAYOUT,
                    titleId = R.string.renderer_screen_layout,
                    choicesId = R.array.rendererScreenLayoutNames,
                    valuesId = R.array.rendererScreenLayoutValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ASPECT_RATIO,
                    titleId = R.string.renderer_aspect_ratio,
                    choicesId = R.array.rendererAspectRatioNames,
                    valuesId = R.array.rendererAspectRatioValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.VERTICAL_ALIGNMENT,
                    titleId = R.string.vertical_alignment,
                    descriptionId = 0,
                    choicesId = R.array.verticalAlignmentEntries,
                    valuesId = R.array.verticalAlignmentValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE,
                    titleId = R.string.use_disk_shader_cache,
                    descriptionId = R.string.use_disk_shader_cache_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_FORCE_MAX_CLOCK,
                    titleId = R.string.renderer_force_max_clock,
                    descriptionId = R.string.renderer_force_max_clock_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS,
                    titleId = R.string.renderer_asynchronous_shaders,
                    descriptionId = R.string.renderer_asynchronous_shaders_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_REACTIVE_FLUSHING,
                    titleId = R.string.renderer_reactive_flushing,
                    descriptionId = R.string.renderer_reactive_flushing_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.MAX_ANISOTROPY,
                    titleId = R.string.anisotropic_filtering,
                    descriptionId = R.string.anisotropic_filtering_description,
                    choicesId = R.array.anisoEntries,
                    valuesId = R.array.anisoValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.AUDIO_OUTPUT_ENGINE,
                    titleId = R.string.audio_output_engine,
                    choicesId = R.array.outputEngineEntries,
                    valuesId = R.array.outputEngineValues
                )
            )
            put(
                SliderSetting(
                    ByteSetting.AUDIO_VOLUME,
                    titleId = R.string.audio_volume,
                    descriptionId = R.string.audio_volume_description,
                    units = "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_BACKEND,
                    titleId = R.string.renderer_api,
                    choicesId = R.array.rendererApiNames,
                    valuesId = R.array.rendererApiValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_DEBUG,
                    titleId = R.string.renderer_debug,
                    descriptionId = R.string.renderer_debug_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.CPU_DEBUG_MODE,
                    titleId = R.string.cpu_debug_mode,
                    descriptionId = R.string.cpu_debug_mode_description
                )
            )

            val fastmem = object : AbstractBooleanSetting {
                override fun getBoolean(needsGlobal: Boolean): Boolean =
                    BooleanSetting.FASTMEM.getBoolean() &&
                        BooleanSetting.FASTMEM_EXCLUSIVES.getBoolean()

                override fun setBoolean(value: Boolean) {
                    BooleanSetting.FASTMEM.setBoolean(value)
                    BooleanSetting.FASTMEM_EXCLUSIVES.setBoolean(value)
                }

                override val key: String = FASTMEM_COMBINED
                override val isRuntimeModifiable: Boolean = false
                override val pairedSettingKey = BooleanSetting.CPU_DEBUG_MODE.key
                override val defaultValue: Boolean = true
                override val isSwitchable: Boolean = true
                override var global: Boolean
                    get() {
                        return BooleanSetting.FASTMEM.global &&
                            BooleanSetting.FASTMEM_EXCLUSIVES.global
                    }
                    set(value) {
                        BooleanSetting.FASTMEM.global = value
                        BooleanSetting.FASTMEM_EXCLUSIVES.global = value
                    }

                override val isSaveable = true

                override fun getValueAsString(needsGlobal: Boolean): String =
                    getBoolean().toString()

                override fun reset() = setBoolean(defaultValue)
            }
            put(SwitchSetting(fastmem, R.string.fastmem))
        }
    }
}
