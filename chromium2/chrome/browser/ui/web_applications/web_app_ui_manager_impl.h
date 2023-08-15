// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class BrowserWindow;
class Profile;

namespace views {
class NativeWindowTracker;
}  // namespace views

namespace web_app {

class WebAppProvider;
class WebAppDialogManager;

// This KeyedService is a UI counterpart for WebAppProvider.
// TODO(calamity): Rename this to WebAppProviderDelegate to better reflect that
// this class serves a wide range of Web Applications <-> Browser purposes.
class WebAppUiManagerImpl : public BrowserListObserver, public WebAppUiManager {
 public:
  static WebAppUiManagerImpl* Get(WebAppProvider* provider);

  explicit WebAppUiManagerImpl(Profile* profile);
  WebAppUiManagerImpl(const WebAppUiManagerImpl&) = delete;
  WebAppUiManagerImpl& operator=(const WebAppUiManagerImpl&) = delete;
  ~WebAppUiManagerImpl() override;

  void Start() override;
  void Shutdown() override;

  WebAppDialogManager& dialog_manager();

  // WebAppUiManager:
  WebAppUiManagerImpl* AsImpl() override;
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;
  bool CanAddAppToQuickLaunchBar() const override;
  void AddAppToQuickLaunchBar(const AppId& app_id) override;
  bool IsAppInQuickLaunchBar(const AppId& app_id) const override;
  bool IsInAppWindow(content::WebContents* web_contents,
                     const AppId* app_id) const override;
  void NotifyOnAssociatedAppChanged(
      content::WebContents* web_contents,
      const absl::optional<AppId>& previous_app_id,
      const absl::optional<AppId>& new_app_id) const override;
  bool CanReparentAppTabToWindow(const AppId& app_id,
                                 bool shortcut_created) const override;
  void ReparentAppTabToWindow(content::WebContents* contents,
                              const AppId& app_id,
                              bool shortcut_created) override;
  void ShowWebAppIdentityUpdateDialog(
      const std::string& app_id,
      bool title_change,
      bool icon_change,
      const std::u16string& old_title,
      const std::u16string& new_title,
      const SkBitmap& old_icon,
      const SkBitmap& new_icon,
      content::WebContents* web_contents,
      web_app::AppIdentityDialogCallback callback) override;
  base::Value LaunchWebApp(apps::AppLaunchParams params,
                           LaunchWebAppWindowSetting launch_setting,
                           Profile& profile,
                           LaunchWebAppCallback callback,
                           AppLock& lock) override;
#if BUILDFLAG(IS_CHROMEOS)
  void MigrateLauncherState(const AppId& from_app_id,
                            const AppId& to_app_id,
                            base::OnceClosure callback) override;

  void DisplayRunOnOsLoginNotification(
      const std::vector<std::string>& app_names,
      base::WeakPtr<Profile> profile) override;
#endif
  content::WebContents* CreateNewTab() override;
  void TriggerInstallDialog(content::WebContents* web_contents) override;

  void PresentUserUninstallDialog(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      BrowserWindow* parent_window,
      UninstallCompleteCallback callback) override;

  void PresentUserUninstallDialog(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback) override;

  void PresentUserUninstallDialog(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      UninstallCompleteCallback callback,
      UninstallScheduledCallback scheduled_callback) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

#if BUILDFLAG(IS_WIN)
  // Attempts to uninstall the given web app id. Meant to be used with OS-level
  // uninstallation support/hooks.
  void UninstallWebAppFromStartupSwitch(const AppId& app_id);
#endif

 private:
  // Returns true if Browser is for an installed App.
  bool IsBrowserForInstalledApp(Browser* browser);

  // Returns AppId of the Browser's installed App, |IsBrowserForInstalledApp|
  // must be true.
  AppId GetAppIdForBrowser(Browser* browser);

  void OnExtensionSystemReady();

  void OnIconsReadForUninstall(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      gfx::NativeWindow parent_window,
      std::unique_ptr<views::NativeWindowTracker> parent_window_tracker,
      UninstallCompleteCallback complete_callback,
      UninstallScheduledCallback uninstall_scheduled_callback,
      std::map<SquareSizePx, SkBitmap> icon_bitmaps);

  void ScheduleUninstallIfUserRequested(
      const AppId& app_id,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCompleteCallback complete_callback,
      UninstallScheduledCallback uninstall_scheduled_callback,
      bool user_wants_uninstall);

  void OnUninstallCancelled(
      UninstallCompleteCallback complete_callback,
      UninstallScheduledCallback uninstall_scheduled_callback);

  const raw_ptr<Profile> profile_;
  std::map<AppId, std::vector<base::OnceClosure>> windows_closed_requests_map_;
  std::map<AppId, size_t> num_windows_for_apps_map_;
  bool started_ = false;

  base::WeakPtrFactory<WebAppUiManagerImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_MANAGER_IMPL_H_
