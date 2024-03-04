// SPDX-FileCopyrightText: 2020 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <QDialog>

class QItemSelection;
class QModelIndex;
class QStandardItemModel;
class QStandardItem;
class QTimer;

namespace Common {
class ParamPackage;
}

namespace InputCommon {
class InputSubsystem;
}

namespace Settings {
struct TouchFromButtonMap;
}

namespace Ui {
class ConfigureTouchFromButton;
}

class ConfigureTouchFromButton : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureTouchFromButton(QWidget* parent,
                                      const std::vector<Settings::TouchFromButtonMap>& touch_maps_,
                                      InputCommon::InputSubsystem* input_subsystem_,
                                      int default_index = 0);
    ~ConfigureTouchFromButton() override;

    int GetSelectedIndex() const;
    std::vector<Settings::TouchFromButtonMap> GetMaps() const;

public slots:
    void ApplyConfiguration();
    void NewBinding(const QPoint& pos);
    void SetActiveBinding(int dot_id);
    void SetCoordinates(int dot_id, const QPoint& pos);

protected:
    void showEvent(QShowEvent* ev) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void NewMapping();
    void DeleteMapping();
    void RenameMapping();
    void EditBinding(const QModelIndex& qi);
    void DeleteBinding();
    void OnBindingSelection(const QItemSelection& selected, const QItemSelection& deselected);
    void OnBindingChanged(QStandardItem* item);
    void OnBindingDeleted(const QModelIndex& parent, int first, int last);

private:
    void SetConfiguration();
    void UpdateUiDisplay();
    void ConnectEvents();
    void GetButtonInput(int row_index, bool is_new);
    void SetPollingResult(const Common::ParamPackage& params, bool cancel);
    void SaveCurrentMapping();

    std::unique_ptr<Ui::ConfigureTouchFromButton> ui;
    std::vector<Settings::TouchFromButtonMap> touch_maps;
    QStandardItemModel* binding_list_model;
    InputCommon::InputSubsystem* input_subsystem;
    int selected_index;

    std::unique_ptr<QTimer> timeout_timer;
    std::unique_ptr<QTimer> poll_timer;
    std::optional<std::function<void(const Common::ParamPackage&, bool)>> input_setter;

    static constexpr int DataRoleDot = Qt::ItemDataRole::UserRole + 2;
};
