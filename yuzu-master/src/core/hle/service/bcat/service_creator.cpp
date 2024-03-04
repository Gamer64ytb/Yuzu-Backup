// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/bcat/bcat_service.h"
#include "core/hle/service/bcat/delivery_cache_storage_service.h"
#include "core/hle/service/bcat/service_creator.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/filesystem/filesystem.h"

namespace Service::BCAT {

std::unique_ptr<BcatBackend> CreateBackendFromSettings([[maybe_unused]] Core::System& system,
                                                       DirectoryGetter getter) {
    return std::make_unique<NullBcatBackend>(std::move(getter));
}

IServiceCreator::IServiceCreator(Core::System& system_, const char* name_)
    : ServiceFramework{system_, name_}, fsc{system.GetFileSystemController()} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&IServiceCreator::CreateBcatService>, "CreateBcatService"},
        {1, D<&IServiceCreator::CreateDeliveryCacheStorageService>, "CreateDeliveryCacheStorageService"},
        {2, D<&IServiceCreator::CreateDeliveryCacheStorageServiceWithApplicationId>, "CreateDeliveryCacheStorageServiceWithApplicationId"},
        {3, nullptr, "CreateDeliveryCacheProgressService"},
        {4, nullptr, "CreateDeliveryCacheProgressServiceWithApplicationId"},
    };
    // clang-format on

    RegisterHandlers(functions);

    backend =
        CreateBackendFromSettings(system_, [this](u64 tid) { return fsc.GetBCATDirectory(tid); });
}

IServiceCreator::~IServiceCreator() = default;

Result IServiceCreator::CreateBcatService(ClientProcessId process_id,
                                          OutInterface<IBcatService> out_interface) {
    LOG_INFO(Service_BCAT, "called, process_id={}", process_id.pid);
    *out_interface = std::make_shared<IBcatService>(system, *backend);
    R_SUCCEED();
}

Result IServiceCreator::CreateDeliveryCacheStorageService(
    ClientProcessId process_id, OutInterface<IDeliveryCacheStorageService> out_interface) {
    LOG_INFO(Service_BCAT, "called, process_id={}", process_id.pid);

    const auto title_id = system.GetApplicationProcessProgramID();
    *out_interface =
        std::make_shared<IDeliveryCacheStorageService>(system, fsc.GetBCATDirectory(title_id));
    R_SUCCEED();
}

Result IServiceCreator::CreateDeliveryCacheStorageServiceWithApplicationId(
    u64 application_id, OutInterface<IDeliveryCacheStorageService> out_interface) {
    LOG_DEBUG(Service_BCAT, "called, application_id={:016X}", application_id);
    *out_interface = std::make_shared<IDeliveryCacheStorageService>(
        system, fsc.GetBCATDirectory(application_id));
    R_SUCCEED();
}

} // namespace Service::BCAT
