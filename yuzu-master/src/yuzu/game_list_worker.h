// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <string>

#include <QList>
#include <QObject>
#include <QRunnable>
#include <QString>

#include "common/thread.h"
#include "yuzu/compatibility_list.h"
#include "yuzu/play_time_manager.h"

namespace Core {
class System;
}

class GameList;
class QStandardItem;

namespace FileSys {
class NCA;
class VfsFilesystem;
} // namespace FileSys

/**
 * Asynchronous worker object for populating the game list.
 * Communicates with other threads through Qt's signal/slot system.
 */
class GameListWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    explicit GameListWorker(std::shared_ptr<FileSys::VfsFilesystem> vfs_,
                            FileSys::ManualContentProvider* provider_,
                            QVector<UISettings::GameDir>& game_dirs_,
                            const CompatibilityList& compatibility_list_,
                            const PlayTime::PlayTimeManager& play_time_manager_,
                            Core::System& system_);
    ~GameListWorker() override;

    /// Starts the processing of directory tree information.
    void run() override;

public:
    /**
     * Synchronously processes any events queued by the worker.
     *
     * AddDirEntry is called on the game list for every discovered directory.
     * AddEntry is called on the game list for every discovered program.
     * DonePopulating is called on the game list when processing completes.
     */
    void ProcessEvents(GameList* game_list);

signals:
    void DataAvailable();

private:
    template <typename F>
    void RecordEvent(F&& func);

private:
    void AddTitlesToGameList(GameListDir* parent_dir);

    enum class ScanTarget {
        FillManualContentProvider,
        PopulateGameList,
    };

    void ScanFileSystem(ScanTarget target, const std::string& dir_path, bool deep_scan,
                        GameListDir* parent_dir);

    std::shared_ptr<FileSys::VfsFilesystem> vfs;
    FileSys::ManualContentProvider* provider;
    QVector<UISettings::GameDir>& game_dirs;
    const CompatibilityList& compatibility_list;
    const PlayTime::PlayTimeManager& play_time_manager;

    QStringList watch_list;

    std::mutex lock;
    std::condition_variable cv;
    std::deque<std::function<void(GameList*)>> queued_events;
    std::atomic_bool stop_requested = false;
    Common::Event processing_completed;

    Core::System& system;
};
