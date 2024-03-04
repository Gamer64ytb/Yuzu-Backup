// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;
class IAppletCommonFunctions;
class IAudioController;
class ICommonStateGetter;
class IDebugFunctions;
class IDisplayController;
class IHomeMenuFunctions;
class IGlobalStateController;
class ILibraryAppletCreator;
class ILibraryAppletSelfAccessor;
class IProcessWindingController;
class ISelfController;
class IWindowController;

class ILibraryAppletProxy final : public ServiceFramework<ILibraryAppletProxy> {
public:
    explicit ILibraryAppletProxy(Core::System& system_, std::shared_ptr<Applet> applet,
                                 Kernel::KProcess* process);
    ~ILibraryAppletProxy();

private:
    Result GetAudioController(Out<SharedPointer<IAudioController>> out_audio_controller);
    Result GetDisplayController(Out<SharedPointer<IDisplayController>> out_display_controller);
    Result GetProcessWindingController(
        Out<SharedPointer<IProcessWindingController>> out_process_winding_controller);
    Result GetDebugFunctions(Out<SharedPointer<IDebugFunctions>> out_debug_functions);
    Result GetWindowController(Out<SharedPointer<IWindowController>> out_window_controller);
    Result GetSelfController(Out<SharedPointer<ISelfController>> out_self_controller);
    Result GetCommonStateGetter(Out<SharedPointer<ICommonStateGetter>> out_common_state_getter);
    Result GetLibraryAppletCreator(
        Out<SharedPointer<ILibraryAppletCreator>> out_library_applet_creator);
    Result OpenLibraryAppletSelfAccessor(
        Out<SharedPointer<ILibraryAppletSelfAccessor>> out_library_applet_self_accessor);
    Result GetAppletCommonFunctions(
        Out<SharedPointer<IAppletCommonFunctions>> out_applet_common_functions);
    Result GetHomeMenuFunctions(Out<SharedPointer<IHomeMenuFunctions>> out_home_menu_functions);
    Result GetGlobalStateController(
        Out<SharedPointer<IGlobalStateController>> out_global_state_controller);

    Kernel::KProcess* const m_process;
    const std::shared_ptr<Applet> m_applet;
};

} // namespace Service::AM
