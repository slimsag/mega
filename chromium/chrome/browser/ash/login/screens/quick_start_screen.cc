// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"
#include <memory>

#include "base/i18n/time_formatting.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash {

namespace {

constexpr const char kUserActionCancelClicked[] = "cancel";
constexpr const char kUserActionWifiConnected[] = "wifi_connected";

}  // namespace

// static
std::string QuickStartScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL:
      return "Cancel";
    case Result::WIFI_CONNECTED:
      return "WifiConnected";
  }
}

QuickStartScreen::QuickStartScreen(base::WeakPtr<TView> view,
                                   const ScreenExitCallback& exit_callback)
    : BaseScreen(QuickStartView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

QuickStartScreen::~QuickStartScreen() {
  UnbindFromBootstrapController();
}

bool QuickStartScreen::MaybeSkip(WizardContext& context) {
  return false;
}

void QuickStartScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();

  // Only for the first time setup.
  if (!bootstrap_controller_) {
    bootstrap_controller_ =
        LoginDisplayHost::default_host()->GetQuickStartBootstrapController();
    bootstrap_controller_->AddObserver(this);
    DetermineDiscoverableName();
  }

  switch (flow_state_) {
    case FlowState::INITIAL:
      bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
      break;
    case FlowState::CONTINUING_AFTER_ENROLLMENT_CHECKS:
      view_->ShowTransferringGaiaCredentials();
      bootstrap_controller_->AttemptGoogleAccountTransfer();
      break;
    case FlowState::RESUMING_AFTER_CRITICAL_UPDATE:
    case FlowState::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void QuickStartScreen::SetFlowState(FlowState flow_state) {
  flow_state_ = flow_state;
}

void QuickStartScreen::HideImpl() {
  if (bootstrap_controller_) {
    bootstrap_controller_->RemoveObserver(this);
  }
  bootstrap_controller_.reset();
}

void QuickStartScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancelClicked) {
    if (bootstrap_controller_) {
      bootstrap_controller_->MaybeCloseOpenConnections();
      bootstrap_controller_->StopAdvertising();
    }
    exit_callback_.Run(Result::CANCEL);
  } else if (action_id == kUserActionWifiConnected) {
    exit_callback_.Run(Result::WIFI_CONNECTED);
  }
}

void QuickStartScreen::OnStatusChanged(
    const quick_start::TargetDeviceBootstrapController::Status& status) {
  using Step = quick_start::TargetDeviceBootstrapController::Step;
  using QRCodePixelData = quick_start::QRCode::PixelData;

  switch (status.step) {
    case Step::ADVERTISING_WITH_QR_CODE: {
      CHECK(absl::holds_alternative<QRCodePixelData>(status.payload));
      if (!view_) {
        return;
      }
      const auto& code = absl::get<QRCodePixelData>(status.payload);
      base::Value::List qr_code_list;
      for (const auto& it : code) {
        qr_code_list.Append(base::Value(static_cast<bool>(it & 1)));
      }
      view_->SetQRCode(std::move(qr_code_list));
      return;
    }
    case Step::PIN_VERIFICATION: {
      CHECK(status.pin.length() == 4);
      view_->SetPIN(status.pin);
      return;
    }
    case Step::GAIA_CREDENTIALS: {
      SavePhoneInstanceID();
      return;
    }
    case Step::ERROR:
      NOTIMPLEMENTED();
      return;
    case Step::CONNECTING_TO_WIFI:
      view_->ShowConnectingToWifi();
      return;
    case Step::CONNECTED_TO_WIFI:
      view_->ShowConnectedToWifi(status.ssid, status.password);
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->quick_start_setup_ongoing = true;
      return;

    case Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS:
      // Intermediate state. Nothing to do.
      CHECK(flow_state_ == FlowState::CONTINUING_AFTER_ENROLLMENT_CHECKS);
      break;
    case Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS:
      CHECK(flow_state_ == FlowState::CONTINUING_AFTER_ENROLLMENT_CHECKS);
      OnTransferredGoogleAccountDetails(status);
      break;
    case Step::NONE:
    case Step::ADVERTISING_WITHOUT_QR_CODE:
    case Step::CONNECTED:
      // TODO(b/282934168): Implement these screens fully
      quick_start::QS_LOG(INFO)
          << "Hit screen which is not implemented. Continuing";
      return;
  }
}

void QuickStartScreen::OnTransferredGoogleAccountDetails(
    const quick_start::TargetDeviceBootstrapController::Status& status) {
  using FidoAssertionInfo = quick_start::FidoAssertionInfo;
  using ErrorCode = quick_start::TargetDeviceBootstrapController::ErrorCode;

  if (absl::holds_alternative<FidoAssertionInfo>(status.payload)) {
    quick_start::QS_LOG(INFO) << "Successfully received FIDO assertion.";
    auto fido_assertion = absl::get<FidoAssertionInfo>(status.payload);
    view_->ShowFidoAssertionReceived(fido_assertion.email);
  } else {
    CHECK(absl::holds_alternative<ErrorCode>(status.payload));
    quick_start::QS_LOG(ERROR)
        << "Error receiving FIDO assertion. Error Code = "
        << static_cast<int>(absl::get<ErrorCode>(status.payload));

    // TODO(b:286873060) - Implement retry mechanism/graceful exit.
    NOTIMPLEMENTED();
  }
}

void QuickStartScreen::DetermineDiscoverableName() {
  CHECK(bootstrap_controller_);
  discoverable_name_ = bootstrap_controller_->GetDiscoverableName();
  if (view_) {
    view_->SetDiscoverableName(discoverable_name_);
  }
}

void QuickStartScreen::UnbindFromBootstrapController() {
  if (!bootstrap_controller_) {
    return;
  }
  bootstrap_controller_->RemoveObserver(this);
  bootstrap_controller_.reset();
}

void QuickStartScreen::SavePhoneInstanceID() {
  if (!bootstrap_controller_) {
    return;
  }

  std::string phone_instance_id = bootstrap_controller_->GetPhoneInstanceId();
  if (phone_instance_id.empty()) {
    return;
  }

  quick_start::QS_LOG(INFO)
      << "Adding Phone Instance ID to Wizard Object for Unified "
         "Setup UI enhancements. quick_start_phone_instance_id: "
      << phone_instance_id;
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->quick_start_phone_instance_id = phone_instance_id;
}

}  // namespace ash
