// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_observer.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_delegate.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Controls the Game Dashboard behavior on supported windows.
class ASH_EXPORT GameDashboardController : public aura::EnvObserver,
                                           public aura::WindowObserver,
                                           public CaptureModeObserver {
 public:
  explicit GameDashboardController(
      std::unique_ptr<GameDashboardDelegate> delegate);
  GameDashboardController(const GameDashboardController&) = delete;
  GameDashboardController& operator=(const GameDashboardController&) = delete;
  ~GameDashboardController() override;

  // Returns the singleton instance owned by `Shell`.
  static GameDashboardController* Get();

  // Checks whether the `window` is a game.
  static bool IsGameWindow(aura::Window* window);

  // Checks whether the `window` can respond to accelerator commands.
  static bool ReadyForAccelerator(aura::Window* window);

  GameDashboardContext* active_recording_context() {
    return active_recording_context_;
  }

  // Returns a pointer to the `GameDashboardContext` if the given `window` is a
  // game window, otherwise nullptr.
  GameDashboardContext* GetGameDashboardContext(aura::Window* window) const;

  // Represents the start of the `context`'s game window capture session.
  // Sets `context` as the `active_recording_context_`, and requests
  // `CaptureModeController` to start a capture session for the `context`'s game
  // window. The session ends when `OnRecordingEnded` or
  // `OnRecordingStartAborted` is called.
  void StartCaptureSession(GameDashboardContext* context);

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // CaptureModeObserver:
  void OnRecordingStarted(aura::Window* current_root) override;
  void OnRecordingEnded() override;
  void OnVideoFileFinalized(bool user_deleted_video_file,
                            const gfx::ImageSkia& thumbnail) override;
  void OnRecordedWindowChangingRoot(aura::Window* new_root) override;
  void OnRecordingStartAborted() override;

 private:
  friend class GameDashboardControllerTest;
  friend class GameDashboardTestBase;

  enum class WindowGameState { kGame, kNotGame, kNotYetKnown };

  // Checks to see if the given window is a game. If there's not enough
  // information, then returns `kNotYetKnown`, otherwise returns `kGame` or
  // `kNotGame`.
  WindowGameState GetWindowGameState(aura::Window* window) const;

  // Updates the window observation, depending on whether the given window is a
  // game or not.
  void RefreshWindowTracking(aura::Window* window);

  // Updates the main menu button state for a game window.
  void RefreshMainMenuButton(aura::Window* window);

  std::map<aura::Window*, std::unique_ptr<GameDashboardContext>>
      game_window_contexts_;

  // The delegate responsible for communicating with between Ash and the Game
  // Dashboard service in the browser.
  std::unique_ptr<GameDashboardDelegate> delegate_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  // Represents the active `GameDashboardContext`. If
  // `active_recording_context_` is non-null, then `CaptureModeController` is
  // recording the game window, or has been requested to record it. Resets
  // when the recording session ends or aborted.
  // Owned by `game_window_contexts_`.
  raw_ptr<GameDashboardContext, ExperimentalAsh> active_recording_context_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONTROLLER_H_
