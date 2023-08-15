// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"

#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "components/google/core/common/google_util.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#import "net/base/mac/url_conversions.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;
using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

@interface ManageSyncSettingsCoordinator () <
    ManageSyncSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate,
    SignoutActionSheetCoordinatorDelegate,
    SyncErrorSettingsCommandHandler,
    SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;
// Mediator.
@property(nonatomic, strong) ManageSyncSettingsMediator* mediator;
// Sync service.
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;
// Authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Displays the sign-out options for a syncing user.
@property(nonatomic, strong)
    SignoutActionSheetCoordinator* signoutActionSheetCoordinator;
@property(nonatomic, assign) BOOL signOutFlowInProgress;

@end

@implementation ManageSyncSettingsCoordinator {
  // Dismiss callback for Web and app setting details view.
  DismissViewCallback _dismissWebAndAppSettingDetailsController;
  // Dismiss callback for account details view.
  DismissViewCallback _dismissAccountDetailsController;
  // The account sync state.
  SyncSettingsAccountState _accountState;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                    accountState:
                                        (SyncSettingsAccountState)accountState {
  if (self = [super initWithBaseViewController:navigationController
                                       browser:browser]) {
    _baseNavigationController = navigationController;
    _accountState = accountState;
  }
  return self;
}

- (void)start {
  DCHECK(self.baseNavigationController);
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  switch (_accountState) {
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
    case SyncSettingsAccountState::kSyncing:
      // Ensure that SyncService::IsSetupInProgress is true while the
      // manage-sync-settings UI is open.
      syncSetupService->PrepareForFirstSyncSetup();
      break;
    case SyncSettingsAccountState::kSignedIn:
      break;
    case SyncSettingsAccountState::kSignedOut:
      NOTREACHED();
      break;
  }

  self.mediator = [[ManageSyncSettingsMediator alloc]
        initWithSyncService:self.syncService
            identityManager:IdentityManagerFactory::GetForBrowserState(
                                browserState)
      authenticationService:self.authService
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browserState)
        initialAccountState:_accountState];
  self.mediator.syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  self.mediator.commandHandler = self;
  self.mediator.syncErrorHandler = self;
  self.mediator.forcedSigninEnabled =
      self.authService->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy;
  self.viewController = [[ManageSyncSettingsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];

  NSString* title = self.mediator.overrideViewControllerTitle;
  if (!title) {
    title = self.delegate.manageSyncSettingsCoordinatorTitle;
  }
  self.viewController.title = title;
  self.viewController.isAccountStateSignedIn =
      _accountState == SyncSettingsAccountState::kSignedIn;
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;
  self.viewController.modelDelegate = self.mediator;
  self.viewController.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());

  self.mediator.consumer = self.viewController;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  _syncObserver = std::make_unique<SyncObserverBridge>(self, self.syncService);
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
  // This coordinator displays the main view and it is in charge to enable sync
  // or not when being closed.
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  // Resets sync blocker if any gets set by PrepareForFirstSyncSetup.
  syncSetupService->CommitSyncChanges();

  _syncObserver.reset();
  [self.signoutActionSheetCoordinator stop];
  _signoutActionSheetCoordinator = nil;
}

#pragma mark - Properties

- (syncer::SyncService*)syncService {
  return SyncServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

#pragma mark - Private

// Closes the Manage sync settings view controller.
- (void)closeManageSyncSettings {
  if (_settingsAreDismissed) {
    return;
  }
  if (self.viewController.navigationController) {
    if (!_dismissWebAndAppSettingDetailsController.is_null()) {
      std::move(_dismissWebAndAppSettingDetailsController)
          .Run(/*animated*/ false);
    }
    if (!_dismissAccountDetailsController.is_null()) {
      std::move(_dismissAccountDetailsController).Run(/*animated=*/false);
    }

    NSEnumerator<UIViewController*>* inversedViewControllers =
        [self.baseNavigationController.viewControllers reverseObjectEnumerator];
    for (UIViewController* controller in inversedViewControllers) {
      if (controller == self.viewController) {
        break;
      }
      if ([controller respondsToSelector:@selector(settingsWillBeDismissed)]) {
        [controller performSelector:@selector(settingsWillBeDismissed)];
      }
    }

    [self.baseNavigationController popToViewController:self.viewController
                                              animated:NO];
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
  _settingsAreDismissed = YES;
}

#pragma mark - ManageSyncSettingsTableViewControllerPresentationDelegate

- (void)manageSyncSettingsTableViewControllerWasRemoved:
    (ManageSyncSettingsTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
}

#pragma mark - ManageSyncSettingsCommandHandler

- (void)openWebAppActivityDialog {
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  _dismissWebAndAppSettingDetailsController =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentWebAndAppSettingDetailsController(identity,
                                                     self.viewController, YES);
}

- (void)openDataFromChromeSyncWebPage {
  if ([self.delegate
          respondsToSelector:@selector
          (manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:)]) {
    [self.delegate
        manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:self];
  }
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

- (void)showTurnOffSyncOptionsFromTargetRect:(CGRect)targetRect {
  self.signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            rect:targetRect
                            view:self.viewController.view
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutSettings];
  self.signoutActionSheetCoordinator.delegate = self;
  __weak ManageSyncSettingsCoordinator* weakSelf = self;
  self.signoutActionSheetCoordinator.completion = ^(BOOL success) {
    if (success) {
      [weakSelf closeManageSyncSettings];
    }
  };
  [self.signoutActionSheetCoordinator start];
}

- (void)signOut {
  if (!self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // This could happen in very rare cases, if the account somehow got removed
    // after the settings UI was created.
    return;
  }

  self.signOutFlowInProgress = YES;
  [self.viewController preventUserInteraction];
  signin_metrics::RecordSignoutUserAction(/*force_clear_data=*/false);
  __weak ManageSyncSettingsCoordinator* weakSelf = self;
  ProceduralBlock signOutCompletion = ^() {
    __strong ManageSyncSettingsCoordinator* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    [strongSelf.viewController allowUserInteraction];
    strongSelf.signOutFlowInProgress = NO;
    [strongSelf.delegate showSignOutToast];
    [strongSelf closeManageSyncSettings];
  };
  self.authService->SignOut(
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
      /*force_clear_browsing_data=*/NO, signOutCompletion);
}

- (void)showAccountsPage {
  AccountsTableViewController* accountsTableViewController =
      [[AccountsTableViewController alloc] initWithBrowser:self.browser
                                 closeSettingsOnAddAccount:NO];

  accountsTableViewController.applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  accountsTableViewController.signoutDismissalByParentCoordinator = YES;
  [self.baseNavigationController pushViewController:accountsTableViewController
                                           animated:YES];
}

- (void)showManageYourGoogleAccount {
  _dismissAccountDetailsController =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentAccountDetailsController(
              self.authService->GetPrimaryIdentity(
                  signin::ConsentLevel::kSignin),
              self.viewController,
              /*animated=*/YES);
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  self.signOutFlowInProgress = YES;
  [self.viewController preventUserInteraction];
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self.viewController allowUserInteraction];
  self.signOutFlowInProgress = NO;
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialog {
  DCHECK(self.mediator.shouldEncryptionItemBeEnabled);
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (self.syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowser:self.browser];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowser:self.browser];
  }
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  controllerToPush.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  [self.baseNavigationController pushViewController:controllerToPush
                                           animated:YES];
}

- (void)openTrustedVaultReauthForFetchKeys {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthForFetchKeysFromViewController:self.viewController
                                                   trigger:
                                                       syncer::
                                                           TrustedVaultUserActionTriggerForUMA::
                                                               kSettings];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          self.viewController
                                                                trigger:
                                                                    syncer::TrustedVaultUserActionTriggerForUMA::
                                                                        kSettings];
}

- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity {
  self.authService->ShowMDMErrorDialogForIdentity(identity);
}

- (void)openPrimaryAccountReauthDialog {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  ShowSigninCommand* signinCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kPrimaryAccountReauth
            accessPoint:AccessPoint::ACCESS_POINT_SETTINGS];
  [applicationCommands showSignin:signinCommand
               baseViewController:self.viewController];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (self.signOutFlowInProgress) {
    return;
  }
  if (!self.syncService->GetDisableReasons().Empty()) {
    [self closeManageSyncSettings];
  }
}

@end
