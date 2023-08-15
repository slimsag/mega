// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"

#include <array>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

InstallIsolatedWebAppCommand::InstallIsolatedWebAppCommand(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppLocation& location,
    const absl::optional<base::Version>& expected_version,
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<WebAppUrlLoader> url_loader,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(base::expected<InstallIsolatedWebAppCommandSuccess,
                                           InstallIsolatedWebAppCommandError>)>
        callback,
    std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper)
    : WebAppCommandTemplate<AppLock>("InstallIsolatedWebAppCommand"),
      lock_description_(
          std::make_unique<AppLockDescription>(url_info.app_id())),
      command_helper_(std::move(command_helper)),
      url_info_(url_info),
      location_(location),
      expected_version_(expected_version),
      web_contents_(std::move(web_contents)),
      url_loader_(std::move(url_loader)),
      optional_keep_alive_(std::move(optional_keep_alive)),
      optional_profile_keep_alive_(std::move(optional_profile_keep_alive)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  CHECK(web_contents_ != nullptr);
  CHECK(!callback.is_null());
  CHECK(optional_profile_keep_alive_ == nullptr ||
        &profile() == optional_profile_keep_alive_->profile());

  callback_ =
      base::BindOnce(
          [](base::expected<InstallIsolatedWebAppCommandSuccess,
                            InstallIsolatedWebAppCommandError> result) {
            webapps::InstallableMetrics::TrackInstallResult(result.has_value());
            return result;
          })
          .Then(std::move(callback));
}

InstallIsolatedWebAppCommand::~InstallIsolatedWebAppCommand() = default;

const LockDescription& InstallIsolatedWebAppCommand::lock_description() const {
  return *lock_description_;
}

base::Value InstallIsolatedWebAppCommand::ToDebugValue() const {
  base::Value::Dict debug_value;
  debug_value.Set("app_id", url_info_.app_id());
  debug_value.Set("origin", url_info_.origin().Serialize());
  debug_value.Set("bundle_id", url_info_.web_bundle_id().id());
  debug_value.Set("bundle_type",
                  static_cast<int>(url_info_.web_bundle_id().type()));
  debug_value.Set("location", IsolatedWebAppLocationAsDebugValue(location_));
  debug_value.Set("expected_version", expected_version_.has_value()
                                          ? expected_version_->GetString()
                                          : "unknown");
  return base::Value(std::move(debug_value));
}

void InstallIsolatedWebAppCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lock_ = std::move(lock);

  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(&InstallIsolatedWebAppCommand::CheckTrustAndSignatures,
                     weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::CreateStoragePartition,
                     weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::LoadInstallUrl, weak_ptr),
      base::BindOnce(
          &InstallIsolatedWebAppCommand::CheckInstallabilityAndRetrieveManifest,
          weak_ptr),
      base::BindOnce(
          &InstallIsolatedWebAppCommand::ValidateManifestAndCreateInstallInfo,
          weak_ptr),
      base::BindOnce(
          &InstallIsolatedWebAppCommand::RetrieveIconsAndPopulateInstallInfo,
          weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::FinalizeInstall, weak_ptr));
}

void InstallIsolatedWebAppCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  command_helper_->CheckTrustAndSignatures(
      location_, &profile(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<void>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::CreateStoragePartition(
    base::OnceClosure next_step_callback) {
  command_helper_->CreateStoragePartitionIfNotPresent(profile());
  std::move(next_step_callback).Run();
}

void InstallIsolatedWebAppCommand::LoadInstallUrl(
    base::OnceClosure next_step_callback) {
  command_helper_->LoadInstallUrl(
      location_, *web_contents_.get(), *url_loader_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<void>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::CheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(IsolatedWebAppInstallCommandHelper::ManifestAndUrl)>
        next_step_callback) {
  command_helper_->CheckInstallabilityAndRetrieveManifest(
      *web_contents_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<
                         IsolatedWebAppInstallCommandHelper::ManifestAndUrl>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::ValidateManifestAndCreateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url) {
  base::expected<WebAppInstallInfo, std::string> install_info =
      command_helper_->ValidateManifestAndCreateInstallInfo(expected_version_,
                                                            manifest_and_url);
  RunNextStepOnSuccess(std::move(next_step_callback), std::move(install_info));
}

void InstallIsolatedWebAppCommand::RetrieveIconsAndPopulateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    WebAppInstallInfo install_info) {
  command_helper_->RetrieveIconsAndPopulateInstallInfo(
      std::move(install_info), *web_contents_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<
                         WebAppInstallInfo>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::FinalizeInstall(WebAppInstallInfo info) {
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::ISOLATED_APP_DEV_INSTALL);
  options.isolated_web_app_location = location_;

  lock_->install_finalizer().FinalizeInstall(
      info, options,
      base::BindOnce(&InstallIsolatedWebAppCommand::OnFinalizeInstall,
                     weak_factory_.GetWeakPtr()));
}

void InstallIsolatedWebAppCommand::OnFinalizeInstall(
    const AppId& unused_app_id,
    webapps::InstallResultCode install_result_code,
    OsHooksErrors unused_os_hooks_errors) {
  if (install_result_code == webapps::InstallResultCode::kSuccessNewInstall) {
    ReportSuccess();
  } else {
    std::stringstream os;
    os << install_result_code;
    ReportFailure(base::StrCat({"Error during finalization: ", os.str()}));
  }
}

void InstallIsolatedWebAppCommand::OnShutdown() {
  // Stop any potential ongoing operations by destroying the `command_helper_`.
  command_helper_.reset();

  // TODO(kuragin): Test cancellation of pending installation during system
  // shutdown.
  ReportFailure("System is shutting down.");
}

void InstallIsolatedWebAppCommand::ReportFailure(base::StringPiece message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_),
                     base::unexpected(InstallIsolatedWebAppCommandError{
                         .message = std::string(message)})));
}

void InstallIsolatedWebAppCommand::ReportSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(std::move(callback_),
                     InstallIsolatedWebAppCommandSuccess{}));
}

Profile& InstallIsolatedWebAppCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
