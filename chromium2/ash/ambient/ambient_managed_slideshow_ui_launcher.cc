// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_managed_slideshow_ui_launcher.h"

#include <vector>

#include "ash/ambient/ambient_managed_photo_controller.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/managed/screensaver_images_policy_handler.h"
#include "ash/ambient/metrics/managed_screensaver_metrics.h"
#include "ash/ambient/model/ambient_slideshow_photo_config.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/login/ui/lock_screen.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {

AmbientManagedSlideshowUiLauncher::AmbientManagedSlideshowUiLauncher(
    AmbientViewDelegateImpl* view_delegate,
    ScreensaverImagesPolicyHandler* policy_handler)
    : photo_controller_(*view_delegate,
                        CreateAmbientManagedSlideshowPhotoConfig()),
      delegate_(view_delegate),
      screensaver_images_policy_handler_(policy_handler) {
  ambient_backend_model_observer_.Observe(
      photo_controller_.ambient_backend_model());
  photo_controller_.SetObserver(this);

  CHECK(screensaver_images_policy_handler_);
  screensaver_images_policy_handler_->SetScreensaverImagesUpdatedCallback(
      base::BindRepeating(
          &AmbientManagedSlideshowUiLauncher::UpdateImageFilePaths,
          weak_factory_.GetWeakPtr()));
}

AmbientManagedSlideshowUiLauncher::~AmbientManagedSlideshowUiLauncher() =
    default;

void AmbientManagedSlideshowUiLauncher::OnImagesReady() {
  CHECK(initialization_callback_);
  std::move(initialization_callback_).Run(/*success=*/true);
  metrics_recorder_.RecordSessionStartupTime();
}

void AmbientManagedSlideshowUiLauncher::OnErrorStateChanged() {
  SetReadyState(ComputeReadyState());
}

void AmbientManagedSlideshowUiLauncher::OnLockStateChanged(bool locked) {
  SetReadyState(ComputeReadyState());
}

void AmbientManagedSlideshowUiLauncher::Initialize(
    InitializationCallback on_done) {
  metrics_recorder_.RecordSessionStart();
  initialization_callback_ = std::move(on_done);
  // TODO(b/281056480): Remove this line and add the login screen visible method
  // to session observer. This is required because if we compute the ready state
  // in the constructor, some of the login screen tests fail as there is no
  // lock/login screen at the time of construction and the ready state is false.
  // This will be a no-op if the ready state is already true.
  SetReadyState(ComputeReadyState());
  photo_controller_.UpdateImageFilePaths(
      screensaver_images_policy_handler_->GetScreensaverImages());
  photo_controller_.StartScreenUpdate();
}

void AmbientManagedSlideshowUiLauncher::UpdateImageFilePaths(
    const std::vector<base::FilePath>& path_to_images) {
  photo_controller_.UpdateImageFilePaths(path_to_images);
}

std::unique_ptr<views::View> AmbientManagedSlideshowUiLauncher::CreateView() {
  return std::make_unique<PhotoView>(delegate_,
                                     /*peripheral_ui_visible=*/false);
}

void AmbientManagedSlideshowUiLauncher::Finalize() {
  photo_controller_.StopScreenUpdate();
  metrics_recorder_.RecordSessionEnd();
}

AmbientBackendModel*
AmbientManagedSlideshowUiLauncher::GetAmbientBackendModel() {
  return photo_controller_.ambient_backend_model();
}

bool AmbientManagedSlideshowUiLauncher::IsActive() {
  return photo_controller_.IsScreenUpdateActive();
}

bool AmbientManagedSlideshowUiLauncher::ComputeReadyState() {
  return LockScreen::HasInstance() &&
         !photo_controller_.HasScreenUpdateErrors();
}

}  // namespace ash
