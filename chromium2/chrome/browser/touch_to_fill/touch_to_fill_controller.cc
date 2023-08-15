// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/touch_to_fill_controller.h"

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_controller_delegate.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "components/password_manager/content/browser/keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/url_formatter/elide_url.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using password_manager::PasskeyCredential;
using password_manager::UiCredential;

std::vector<UiCredential> SortCredentials(
    base::span<const UiCredential> credentials) {
  std::vector<UiCredential> result(credentials.begin(), credentials.end());
  // Sort `credentials` according to the following criteria:
  // 1) Prefer exact matches then affiliated, then PSL matches.
  // 2) Prefer credentials that were used recently over others.
  //
  // Note: This ordering matches password_manager_util::FindBestMatches().
  base::ranges::sort(result, std::greater<>{}, [](const UiCredential& cred) {
    return std::make_pair(-static_cast<int>(cred.match_type()),
                          cred.last_used());
  });

  return result;
}

}  // namespace

TouchToFillController::TouchToFillController(
    base::WeakPtr<
        password_manager::KeyboardReplacingSurfaceVisibilityController>
        visibility_controller)
    : visibility_controller_(visibility_controller) {}
TouchToFillController::~TouchToFillController() = default;

void TouchToFillController::Show(
    base::span<const UiCredential> credentials,
    base::span<PasskeyCredential> passkey_credentials,
    std::unique_ptr<TouchToFillControllerDelegate> delegate,
    base::WeakPtr<password_manager::ContentPasswordManagerDriver>
        frame_driver) {
  DCHECK(!delegate_);
  delegate_ = std::move(delegate);
  visibility_controller_->SetVisible(std::move(frame_driver));

  delegate_->OnShow(credentials, passkey_credentials);
  if (credentials.empty() && passkey_credentials.empty()) {
    // Ideally this should never happen. However, in case we do end up invoking
    // Show() without credentials, we should not show Touch To Fill to the user
    // and treat this case as dismissal, in order to restore the soft keyboard.
    OnDismiss();
    return;
  }

  if (!view_)
    view_ = TouchToFillViewFactory::Create(this);

  int flags = TouchToFillView::kNone;
  if (delegate_->ShouldTriggerSubmission()) {
    flags |= TouchToFillView::kTriggerSubmission;
  }
  if (password_manager_launcher::CanManagePasswordsWhenPasskeysPresent()) {
    flags |= TouchToFillView::kCanManagePasswordsWhenPasskeysPresent;
  }
  if (delegate_->ShouldShowHybridOption()) {
    flags |= TouchToFillView::kShouldShowHybridOption;
  }

  GURL url = delegate_->GetFrameUrl();
  view_->Show(
      url,
      TouchToFillView::IsOriginSecure(
          network::IsOriginPotentiallyTrustworthy(url::Origin::Create(url))),
      SortCredentials(credentials), passkey_credentials, flags);
}

void TouchToFillController::OnCredentialSelected(
    const UiCredential& credential) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnCredentialSelected(
      credential, base::BindOnce(&TouchToFillController::ActionCompleted,
                                 base::Unretained(this)));
}

void TouchToFillController::OnPasskeyCredentialSelected(
    const PasskeyCredential& credential) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnPasskeyCredentialSelected(
      credential, base::BindOnce(&TouchToFillController::ActionCompleted,
                                 base::Unretained(this)));
}

void TouchToFillController::OnManagePasswordsSelected(bool passkeys_shown) {
  view_.reset();
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnManagePasswordsSelected(
      passkeys_shown, base::BindOnce(&TouchToFillController::ActionCompleted,
                                     base::Unretained(this)));
}

void TouchToFillController::OnHybridSignInSelected() {
  view_.reset();
  delegate_->OnHybridSignInSelected(base::BindOnce(
      &TouchToFillController::ActionCompleted, base::Unretained(this)));
}

void TouchToFillController::OnDismiss() {
  view_.reset();
  if (!delegate_) {
    // TODO(crbug/1462532): Remove this check when
    // PasswordSuggestionBottomSheetV2 is launched
    return;
  }
  // Unretained is safe here because TouchToFillController owns the delegate.
  delegate_->OnDismiss(base::BindOnce(&TouchToFillController::ActionCompleted,
                                      base::Unretained(this)));
}

gfx::NativeView TouchToFillController::GetNativeView() {
  return delegate_->GetNativeView();
}

void TouchToFillController::Close() {
  // TODO(crbug/1468487). This is a duplicate of `OnDismiss`. Merge the two
  // functions.
  OnDismiss();
}

void TouchToFillController::Reset() {
  if (!visibility_controller_) {
    return;
  }
  if (visibility_controller_->IsVisible()) {
    Close();
  }
  visibility_controller_->Reset();
}

void TouchToFillController::ActionCompleted() {
  if (visibility_controller_) {
    visibility_controller_->SetShown();
  }
  delegate_.reset();
}
