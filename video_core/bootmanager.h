// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <QImage>
#include <QThread>
#include <QTouchEvent>
#include <QWidget>
#include <QWindow>

#include "common/thread.h"
#include "core/frontend/emu_window.h"
#include "input_common/main.h"
#include "video_core/gpu.h"

class GRenderWindow;
class GMainWindow;
class QKeyEvent;
class QStringList;

namespace Core {
enum class SystemResultStatus : u32;
} // namespace Core

namespace InputCommon {
class InputSubsystem;
}

namespace MouseInput {
enum class MouseButton;
}

namespace VideoCore {
enum class LoadCallbackStage;
class RendererBase;
} // namespace VideoCore

class GRenderWindow : public QWidget, public Core::Frontend::EmuWindow {
    Q_OBJECT

public:
    explicit GRenderWindow(Tegra::GPU& gpu_);
    ~GRenderWindow() override;

    void ToggleFullscreen();
    void ShowFullscreen();
    void HideFullscreen();

    // EmuWindow implementation.
    void OnFrameDisplayed() override;
    bool IsShown() const override;
    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;

    void BackupGeometry();
    void RestoreGeometry();
    void restoreGeometry(const QByteArray& geometry); // overridden
    QByteArray saveGeometry();                        // overridden

    qreal windowPixelRatio() const;

    void closeEvent(QCloseEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    /// Converts a Qt mouse button into MouseInput mouse button
    static MouseInput::MouseButton QtButtonToMouseButton(Qt::MouseButton button);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    bool event(QEvent* event) override;

    void focusOutEvent(QFocusEvent* event) override;

    bool InitRenderTarget();

    /// Destroy the previous run's child_widget which should also destroy the child_window
    void ReleaseRenderTarget();

    bool IsLoadingComplete() const;

    void CaptureScreenshot(u32 res_scale, const QString& screenshot_path);

    std::pair<u32, u32> ScaleTouch(const QPointF& pos) const;

    /**
     * Instructs the window to re-launch the application using the specified program_index.
     * @param program_index Specifies the index within the application of the program to launch.
     */
    void ExecuteProgram(std::size_t program_index);

    /// Instructs the window to exit the application.
    void Exit();

    /**
     * If the emulation is running,
     * asks the user if he really want to close the emulator
     *
     * @return true if the user confirmed
     */
    bool ConfirmForceLockedExit();

    void SetExitLock(bool locked) {
        exit_lock.store(locked, std::memory_order::relaxed);
    }

public slots:
    void OnFramebufferSizeChanged();

signals:
    /// Emitted when the window is closed
    void Closed();
    void FirstFrameDisplayed();
    void ExecuteProgramSignal(std::size_t program_index);
    void ExitSignal();
    void MouseActivity();

private:
    void TouchBeginEvent(const QTouchEvent* event);
    void TouchUpdateEvent(const QTouchEvent* event);
    void TouchEndEvent();

    bool TouchStart(const QTouchEvent::TouchPoint& touch_point);
    bool TouchUpdate(const QTouchEvent::TouchPoint& touch_point);
    bool TouchExist(std::size_t id, const QList<QTouchEvent::TouchPoint>& touch_points) const;

    void OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) override;

    bool InitializeOpenGL();
    bool InitializeVulkan();
    bool LoadOpenGL();
    QStringList GetUnsupportedGLExtensions() const;

    InputCommon::InputSubsystem input_subsystem;

    // Main context that will be shared with all other contexts that are requested.
    // If this is used in a shared context setting, then this should not be used directly, but
    // should instead be shared from
    std::shared_ptr<Core::Frontend::GraphicsContext> main_context;

    /// Temporary storage of the screenshot taken
    QImage screenshot_image;

    QByteArray geometry;

    QWidget* child_widget = nullptr;

    bool first_frame = false;

    bool is_fullscreen = false;

    std::atomic_bool exit_lock{false};

    std::array<std::size_t, 16> touch_ids{};

    Tegra::GPU& gpu;

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;
};
