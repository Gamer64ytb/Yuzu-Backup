// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "applets/software_keyboard.h"
#include "common/android/id_cache.h"
#include "common/assert.h"
#include "common/fs/fs_android.h"
#include "video_core/rasterizer_interface.h"

static JavaVM* s_java_vm;
static jclass s_native_library_class;
static jclass s_disk_cache_progress_class;
static jclass s_load_callback_stage_class;
static jclass s_game_dir_class;
static jmethodID s_game_dir_constructor;
static jmethodID s_exit_emulation_activity;
static jmethodID s_disk_cache_load_progress;
static jmethodID s_on_emulation_started;
static jmethodID s_on_emulation_stopped;
static jmethodID s_on_program_changed;

static jclass s_game_class;
static jmethodID s_game_constructor;
static jfieldID s_game_title_field;
static jfieldID s_game_path_field;
static jfieldID s_game_program_id_field;
static jfieldID s_game_developer_field;
static jfieldID s_game_version_field;
static jfieldID s_game_is_homebrew_field;

static jclass s_string_class;
static jclass s_pair_class;
static jmethodID s_pair_constructor;
static jfieldID s_pair_first_field;
static jfieldID s_pair_second_field;

static jclass s_overlay_control_data_class;
static jmethodID s_overlay_control_data_constructor;
static jfieldID s_overlay_control_data_id_field;
static jfieldID s_overlay_control_data_enabled_field;
static jfieldID s_overlay_control_data_landscape_position_field;
static jfieldID s_overlay_control_data_portrait_position_field;
static jfieldID s_overlay_control_data_foldable_position_field;

static jclass s_patch_class;
static jmethodID s_patch_constructor;
static jfieldID s_patch_enabled_field;
static jfieldID s_patch_name_field;
static jfieldID s_patch_version_field;
static jfieldID s_patch_type_field;
static jfieldID s_patch_program_id_field;
static jfieldID s_patch_title_id_field;

static jclass s_double_class;
static jmethodID s_double_constructor;
static jfieldID s_double_value_field;

static jclass s_integer_class;
static jmethodID s_integer_constructor;
static jfieldID s_integer_value_field;

static jclass s_boolean_class;
static jmethodID s_boolean_constructor;
static jfieldID s_boolean_value_field;

static jclass s_player_input_class;
static jmethodID s_player_input_constructor;
static jfieldID s_player_input_connected_field;
static jfieldID s_player_input_buttons_field;
static jfieldID s_player_input_analogs_field;
static jfieldID s_player_input_motions_field;
static jfieldID s_player_input_vibration_enabled_field;
static jfieldID s_player_input_vibration_strength_field;
static jfieldID s_player_input_body_color_left_field;
static jfieldID s_player_input_body_color_right_field;
static jfieldID s_player_input_button_color_left_field;
static jfieldID s_player_input_button_color_right_field;
static jfieldID s_player_input_profile_name_field;
static jfieldID s_player_input_use_system_vibrator_field;

static jclass s_yuzu_input_device_interface;
static jmethodID s_yuzu_input_device_get_name;
static jmethodID s_yuzu_input_device_get_guid;
static jmethodID s_yuzu_input_device_get_port;
static jmethodID s_yuzu_input_device_get_supports_vibration;
static jmethodID s_yuzu_input_device_vibrate;
static jmethodID s_yuzu_input_device_get_axes;
static jmethodID s_yuzu_input_device_has_keys;

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

namespace Common::Android {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                s_java_vm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                s_java_vm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

jclass GetNativeLibraryClass() {
    return s_native_library_class;
}

jclass GetDiskCacheProgressClass() {
    return s_disk_cache_progress_class;
}

jclass GetDiskCacheLoadCallbackStageClass() {
    return s_load_callback_stage_class;
}

jclass GetGameDirClass() {
    return s_game_dir_class;
}

jmethodID GetGameDirConstructor() {
    return s_game_dir_constructor;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

jmethodID GetDiskCacheLoadProgress() {
    return s_disk_cache_load_progress;
}

jmethodID GetOnEmulationStarted() {
    return s_on_emulation_started;
}

jmethodID GetOnEmulationStopped() {
    return s_on_emulation_stopped;
}

jmethodID GetOnProgramChanged() {
    return s_on_program_changed;
}

jclass GetGameClass() {
    return s_game_class;
}

jmethodID GetGameConstructor() {
    return s_game_constructor;
}

jfieldID GetGameTitleField() {
    return s_game_title_field;
}

jfieldID GetGamePathField() {
    return s_game_path_field;
}

jfieldID GetGameProgramIdField() {
    return s_game_program_id_field;
}

jfieldID GetGameDeveloperField() {
    return s_game_developer_field;
}

jfieldID GetGameVersionField() {
    return s_game_version_field;
}

jfieldID GetGameIsHomebrewField() {
    return s_game_is_homebrew_field;
}

jclass GetStringClass() {
    return s_string_class;
}

jclass GetPairClass() {
    return s_pair_class;
}

jmethodID GetPairConstructor() {
    return s_pair_constructor;
}

jfieldID GetPairFirstField() {
    return s_pair_first_field;
}

jfieldID GetPairSecondField() {
    return s_pair_second_field;
}

jclass GetOverlayControlDataClass() {
    return s_overlay_control_data_class;
}

jmethodID GetOverlayControlDataConstructor() {
    return s_overlay_control_data_constructor;
}

jfieldID GetOverlayControlDataIdField() {
    return s_overlay_control_data_id_field;
}

jfieldID GetOverlayControlDataEnabledField() {
    return s_overlay_control_data_enabled_field;
}

jfieldID GetOverlayControlDataLandscapePositionField() {
    return s_overlay_control_data_landscape_position_field;
}

jfieldID GetOverlayControlDataPortraitPositionField() {
    return s_overlay_control_data_portrait_position_field;
}

jfieldID GetOverlayControlDataFoldablePositionField() {
    return s_overlay_control_data_foldable_position_field;
}

jclass GetPatchClass() {
    return s_patch_class;
}

jmethodID GetPatchConstructor() {
    return s_patch_constructor;
}

jfieldID GetPatchEnabledField() {
    return s_patch_enabled_field;
}

jfieldID GetPatchNameField() {
    return s_patch_name_field;
}

jfieldID GetPatchVersionField() {
    return s_patch_version_field;
}

jfieldID GetPatchTypeField() {
    return s_patch_type_field;
}

jfieldID GetPatchProgramIdField() {
    return s_patch_program_id_field;
}

jfieldID GetPatchTitleIdField() {
    return s_patch_title_id_field;
}

jclass GetDoubleClass() {
    return s_double_class;
}

jmethodID GetDoubleConstructor() {
    return s_double_constructor;
}

jfieldID GetDoubleValueField() {
    return s_double_value_field;
}

jclass GetIntegerClass() {
    return s_integer_class;
}

jmethodID GetIntegerConstructor() {
    return s_integer_constructor;
}

jfieldID GetIntegerValueField() {
    return s_integer_value_field;
}

jclass GetBooleanClass() {
    return s_boolean_class;
}

jmethodID GetBooleanConstructor() {
    return s_boolean_constructor;
}

jfieldID GetBooleanValueField() {
    return s_boolean_value_field;
}

jclass GetPlayerInputClass() {
    return s_player_input_class;
}

jmethodID GetPlayerInputConstructor() {
    return s_player_input_constructor;
}

jfieldID GetPlayerInputConnectedField() {
    return s_player_input_connected_field;
}

jfieldID GetPlayerInputButtonsField() {
    return s_player_input_buttons_field;
}

jfieldID GetPlayerInputAnalogsField() {
    return s_player_input_analogs_field;
}

jfieldID GetPlayerInputMotionsField() {
    return s_player_input_motions_field;
}

jfieldID GetPlayerInputVibrationEnabledField() {
    return s_player_input_vibration_enabled_field;
}

jfieldID GetPlayerInputVibrationStrengthField() {
    return s_player_input_vibration_strength_field;
}

jfieldID GetPlayerInputBodyColorLeftField() {
    return s_player_input_body_color_left_field;
}

jfieldID GetPlayerInputBodyColorRightField() {
    return s_player_input_body_color_right_field;
}

jfieldID GetPlayerInputButtonColorLeftField() {
    return s_player_input_button_color_left_field;
}

jfieldID GetPlayerInputButtonColorRightField() {
    return s_player_input_button_color_right_field;
}

jfieldID GetPlayerInputProfileNameField() {
    return s_player_input_profile_name_field;
}

jfieldID GetPlayerInputUseSystemVibratorField() {
    return s_player_input_use_system_vibrator_field;
}

jclass GetYuzuInputDeviceInterface() {
    return s_yuzu_input_device_interface;
}

jmethodID GetYuzuDeviceGetName() {
    return s_yuzu_input_device_get_name;
}

jmethodID GetYuzuDeviceGetGUID() {
    return s_yuzu_input_device_get_guid;
}

jmethodID GetYuzuDeviceGetPort() {
    return s_yuzu_input_device_get_port;
}

jmethodID GetYuzuDeviceGetSupportsVibration() {
    return s_yuzu_input_device_get_supports_vibration;
}

jmethodID GetYuzuDeviceVibrate() {
    return s_yuzu_input_device_vibrate;
}

jmethodID GetYuzuDeviceGetAxes() {
    return s_yuzu_input_device_get_axes;
}

jmethodID GetYuzuDeviceHasKeys() {
    return s_yuzu_input_device_has_keys;
}

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    s_java_vm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
        return JNI_ERR;

    // Initialize Java classes
    const jclass native_library_class = env->FindClass("org/yuzu/yuzu_emu/NativeLibrary");
    s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
    s_disk_cache_progress_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress")));
    s_load_callback_stage_class = reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass(
        "org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress$LoadCallbackStage")));

    const jclass game_dir_class = env->FindClass("org/yuzu/yuzu_emu/model/GameDir");
    s_game_dir_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_dir_class));
    s_game_dir_constructor = env->GetMethodID(game_dir_class, "<init>", "(Ljava/lang/String;Z)V");
    env->DeleteLocalRef(game_dir_class);

    // Initialize methods
    s_exit_emulation_activity =
        env->GetStaticMethodID(s_native_library_class, "exitEmulationActivity", "(I)V");
    s_disk_cache_load_progress =
        env->GetStaticMethodID(s_disk_cache_progress_class, "loadProgress", "(III)V");
    s_on_emulation_started =
        env->GetStaticMethodID(s_native_library_class, "onEmulationStarted", "()V");
    s_on_emulation_stopped =
        env->GetStaticMethodID(s_native_library_class, "onEmulationStopped", "(I)V");
    s_on_program_changed =
        env->GetStaticMethodID(s_native_library_class, "onProgramChanged", "(I)V");

    const jclass game_class = env->FindClass("org/yuzu/yuzu_emu/model/Game");
    s_game_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_class));
    s_game_constructor = env->GetMethodID(game_class, "<init>",
                                          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
                                          "String;Ljava/lang/String;Ljava/lang/String;Z)V");
    s_game_title_field = env->GetFieldID(game_class, "title", "Ljava/lang/String;");
    s_game_path_field = env->GetFieldID(game_class, "path", "Ljava/lang/String;");
    s_game_program_id_field = env->GetFieldID(game_class, "programId", "Ljava/lang/String;");
    s_game_developer_field = env->GetFieldID(game_class, "developer", "Ljava/lang/String;");
    s_game_version_field = env->GetFieldID(game_class, "version", "Ljava/lang/String;");
    s_game_is_homebrew_field = env->GetFieldID(game_class, "isHomebrew", "Z");
    env->DeleteLocalRef(game_class);

    const jclass string_class = env->FindClass("java/lang/String");
    s_string_class = reinterpret_cast<jclass>(env->NewGlobalRef(string_class));
    env->DeleteLocalRef(string_class);

    const jclass pair_class = env->FindClass("kotlin/Pair");
    s_pair_class = reinterpret_cast<jclass>(env->NewGlobalRef(pair_class));
    s_pair_constructor =
        env->GetMethodID(pair_class, "<init>", "(Ljava/lang/Object;Ljava/lang/Object;)V");
    s_pair_first_field = env->GetFieldID(pair_class, "first", "Ljava/lang/Object;");
    s_pair_second_field = env->GetFieldID(pair_class, "second", "Ljava/lang/Object;");
    env->DeleteLocalRef(pair_class);

    const jclass overlay_control_data_class =
        env->FindClass("org/yuzu/yuzu_emu/overlay/model/OverlayControlData");
    s_overlay_control_data_class =
        reinterpret_cast<jclass>(env->NewGlobalRef(overlay_control_data_class));
    s_overlay_control_data_constructor =
        env->GetMethodID(overlay_control_data_class, "<init>",
                         "(Ljava/lang/String;ZLkotlin/Pair;Lkotlin/Pair;Lkotlin/Pair;)V");
    s_overlay_control_data_id_field =
        env->GetFieldID(overlay_control_data_class, "id", "Ljava/lang/String;");
    s_overlay_control_data_enabled_field =
        env->GetFieldID(overlay_control_data_class, "enabled", "Z");
    s_overlay_control_data_landscape_position_field =
        env->GetFieldID(overlay_control_data_class, "landscapePosition", "Lkotlin/Pair;");
    s_overlay_control_data_portrait_position_field =
        env->GetFieldID(overlay_control_data_class, "portraitPosition", "Lkotlin/Pair;");
    s_overlay_control_data_foldable_position_field =
        env->GetFieldID(overlay_control_data_class, "foldablePosition", "Lkotlin/Pair;");
    env->DeleteLocalRef(overlay_control_data_class);

    const jclass patch_class = env->FindClass("org/yuzu/yuzu_emu/model/Patch");
    s_patch_class = reinterpret_cast<jclass>(env->NewGlobalRef(patch_class));
    s_patch_constructor = env->GetMethodID(
        patch_class, "<init>",
        "(ZLjava/lang/String;Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)V");
    s_patch_enabled_field = env->GetFieldID(patch_class, "enabled", "Z");
    s_patch_name_field = env->GetFieldID(patch_class, "name", "Ljava/lang/String;");
    s_patch_version_field = env->GetFieldID(patch_class, "version", "Ljava/lang/String;");
    s_patch_type_field = env->GetFieldID(patch_class, "type", "I");
    s_patch_program_id_field = env->GetFieldID(patch_class, "programId", "Ljava/lang/String;");
    s_patch_title_id_field = env->GetFieldID(patch_class, "titleId", "Ljava/lang/String;");
    env->DeleteLocalRef(patch_class);

    const jclass double_class = env->FindClass("java/lang/Double");
    s_double_class = reinterpret_cast<jclass>(env->NewGlobalRef(double_class));
    s_double_constructor = env->GetMethodID(double_class, "<init>", "(D)V");
    s_double_value_field = env->GetFieldID(double_class, "value", "D");
    env->DeleteLocalRef(double_class);

    const jclass int_class = env->FindClass("java/lang/Integer");
    s_integer_class = reinterpret_cast<jclass>(env->NewGlobalRef(int_class));
    s_integer_constructor = env->GetMethodID(int_class, "<init>", "(I)V");
    s_integer_value_field = env->GetFieldID(int_class, "value", "I");
    env->DeleteLocalRef(int_class);

    const jclass boolean_class = env->FindClass("java/lang/Boolean");
    s_boolean_class = reinterpret_cast<jclass>(env->NewGlobalRef(boolean_class));
    s_boolean_constructor = env->GetMethodID(boolean_class, "<init>", "(Z)V");
    s_boolean_value_field = env->GetFieldID(boolean_class, "value", "Z");
    env->DeleteLocalRef(boolean_class);

    const jclass player_input_class =
        env->FindClass("org/yuzu/yuzu_emu/features/input/model/PlayerInput");
    s_player_input_class = reinterpret_cast<jclass>(env->NewGlobalRef(player_input_class));
    s_player_input_constructor = env->GetMethodID(
        player_input_class, "<init>",
        "(Z[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;ZIJJJJLjava/lang/String;Z)V");
    s_player_input_connected_field = env->GetFieldID(player_input_class, "connected", "Z");
    s_player_input_buttons_field =
        env->GetFieldID(player_input_class, "buttons", "[Ljava/lang/String;");
    s_player_input_analogs_field =
        env->GetFieldID(player_input_class, "analogs", "[Ljava/lang/String;");
    s_player_input_motions_field =
        env->GetFieldID(player_input_class, "motions", "[Ljava/lang/String;");
    s_player_input_vibration_enabled_field =
        env->GetFieldID(player_input_class, "vibrationEnabled", "Z");
    s_player_input_vibration_strength_field =
        env->GetFieldID(player_input_class, "vibrationStrength", "I");
    s_player_input_body_color_left_field =
        env->GetFieldID(player_input_class, "bodyColorLeft", "J");
    s_player_input_body_color_right_field =
        env->GetFieldID(player_input_class, "bodyColorRight", "J");
    s_player_input_button_color_left_field =
        env->GetFieldID(player_input_class, "buttonColorLeft", "J");
    s_player_input_button_color_right_field =
        env->GetFieldID(player_input_class, "buttonColorRight", "J");
    s_player_input_profile_name_field =
        env->GetFieldID(player_input_class, "profileName", "Ljava/lang/String;");
    s_player_input_use_system_vibrator_field =
        env->GetFieldID(player_input_class, "useSystemVibrator", "Z");
    env->DeleteLocalRef(player_input_class);

    const jclass yuzu_input_device_interface =
        env->FindClass("org/yuzu/yuzu_emu/features/input/YuzuInputDevice");
    s_yuzu_input_device_interface =
        reinterpret_cast<jclass>(env->NewGlobalRef(yuzu_input_device_interface));
    s_yuzu_input_device_get_name =
        env->GetMethodID(yuzu_input_device_interface, "getName", "()Ljava/lang/String;");
    s_yuzu_input_device_get_guid =
        env->GetMethodID(yuzu_input_device_interface, "getGUID", "()Ljava/lang/String;");
    s_yuzu_input_device_get_port = env->GetMethodID(yuzu_input_device_interface, "getPort", "()I");
    s_yuzu_input_device_get_supports_vibration =
        env->GetMethodID(yuzu_input_device_interface, "getSupportsVibration", "()Z");
    s_yuzu_input_device_vibrate = env->GetMethodID(yuzu_input_device_interface, "vibrate", "(F)V");
    s_yuzu_input_device_get_axes =
        env->GetMethodID(yuzu_input_device_interface, "getAxes", "()[Ljava/lang/Integer;");
    s_yuzu_input_device_has_keys =
        env->GetMethodID(yuzu_input_device_interface, "hasKeys", "([I)[Z");
    env->DeleteLocalRef(yuzu_input_device_interface);

    // Initialize Android Storage
    Common::FS::Android::RegisterCallbacks(env, s_native_library_class);

    // Initialize applets
    Common::Android::SoftwareKeyboard::InitJNI(env);

    return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
        return;
    }

    // UnInitialize Android Storage
    Common::FS::Android::UnRegisterCallbacks();
    env->DeleteGlobalRef(s_native_library_class);
    env->DeleteGlobalRef(s_disk_cache_progress_class);
    env->DeleteGlobalRef(s_load_callback_stage_class);
    env->DeleteGlobalRef(s_game_dir_class);
    env->DeleteGlobalRef(s_game_class);
    env->DeleteGlobalRef(s_string_class);
    env->DeleteGlobalRef(s_pair_class);
    env->DeleteGlobalRef(s_overlay_control_data_class);
    env->DeleteGlobalRef(s_patch_class);
    env->DeleteGlobalRef(s_double_class);
    env->DeleteGlobalRef(s_integer_class);
    env->DeleteGlobalRef(s_boolean_class);
    env->DeleteGlobalRef(s_player_input_class);
    env->DeleteGlobalRef(s_yuzu_input_device_interface);

    // UnInitialize applets
    SoftwareKeyboard::CleanupJNI(env);
}

#ifdef __cplusplus
}
#endif

} // namespace Common::Android
