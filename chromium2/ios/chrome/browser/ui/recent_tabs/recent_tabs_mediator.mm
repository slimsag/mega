// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"

#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/synced_session.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_consumer.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

namespace {

// Returns whether the user needs to enter a passphrase or enable sync to make
// tab sync work.
bool UserActionIsRequiredToHaveTabSyncWork(syncer::SyncService* sync_service) {
  if (!sync_service->GetDisableReasons().Empty()) {
    return true;
  }

  if (!sync_service->IsSyncFeatureEnabled() &&
      !base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return true;
  }

  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs)) {
    return true;
  }

  switch (sync_service->GetUserActionableError()) {
    // No error.
    case syncer::SyncService::UserActionableError::kNone:
      return false;

    // These errors effectively amount to disabled sync or effectively paused.
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::kGenericUnrecoverableError:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return true;

    // This error doesn't stop tab sync.
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return false;

    // These errors don't actually stop sync.
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return false;
  }

  NOTREACHED_NORETURN();
}

}  // namespace

@interface RecentTabsMediator () <SyncedSessionsObserver,
                                  WebStateListObserving> {
  std::unique_ptr<AllWebStateListObservationRegistrar> _registrar;
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserver;
  std::unique_ptr<recent_tabs::ClosedTabsObserverBridge> _closedTabsObserver;
  SessionsSyncUserState _userState;
  // The list of web state list currently processing batch operations (e.g.
  // Closing All, or Undoing a Close All).
  std::set<WebStateList*> _webStateListsWithBatchOperations;
}

// Return the user's current sign-in and chrome-sync state.
- (SessionsSyncUserState)userSignedInState;
// Reload the panel.
- (void)refreshSessionsView;
@property(nonatomic, assign)
    sync_sessions::SessionSyncService* sessionSyncService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;
@property(nonatomic, assign) sessions::TabRestoreService* restoreService;
@property(nonatomic, assign) FaviconLoader* faviconLoader;
@property(nonatomic, assign) syncer::SyncService* syncService;
@property(nonatomic, assign) BrowserList* browserList;

@end

@implementation RecentTabsMediator

- (instancetype)
    initWithSessionSyncService:
        (sync_sessions::SessionSyncService*)sessionSyncService
               identityManager:(signin::IdentityManager*)identityManager
                restoreService:(sessions::TabRestoreService*)restoreService
                 faviconLoader:(FaviconLoader*)faviconLoader
                   syncService:(syncer::SyncService*)syncService
                   browserList:(BrowserList*)browserList {
  self = [super init];
  if (self) {
    _sessionSyncService = sessionSyncService;
    _identityManager = identityManager;
    _restoreService = restoreService;
    _faviconLoader = faviconLoader;
    _syncService = syncService;
    _browserList = browserList;
  }
  return self;
}

#pragma mark - Public Interface

- (void)initObservers {
  if (!_registrar) {
    _registrar = std::make_unique<AllWebStateListObservationRegistrar>(
        _browserList, std::make_unique<WebStateListObserverBridge>(self),
        AllWebStateListObservationRegistrar::Mode::REGULAR);
  }
  if (!_syncedSessionsObserver) {
    _syncedSessionsObserver =
        std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
            self, self.identityManager, self.sessionSyncService);
  }
  if (!_closedTabsObserver) {
    _closedTabsObserver =
        std::make_unique<recent_tabs::ClosedTabsObserverBridge>(self);
    if (self.restoreService) {
      self.restoreService->AddObserver(_closedTabsObserver.get());
    }
    [self.consumer setTabRestoreService:self.restoreService];
  }
}

- (void)disconnect {
  _registrar.reset();
  _syncedSessionsObserver.reset();

  if (_closedTabsObserver) {
    if (self.restoreService) {
      self.restoreService->RemoveObserver(_closedTabsObserver.get());
    }
    _closedTabsObserver.reset();
    _sessionSyncService = nullptr;
    _identityManager = nullptr;
    _restoreService = nullptr;
    _faviconLoader = nullptr;
    _syncService = nullptr;
  }
}

- (void)configureConsumer {
  [self refreshSessionsView];
}

#pragma mark - SyncedSessionsObserver

- (void)reloadSessions {
  [self refreshSessionsView];
}

- (void)onSyncStateChanged {
  [self refreshSessionsView];
}

#pragma mark - ClosedTabsObserving

- (void)tabRestoreServiceChanged:(sessions::TabRestoreService*)service {
  self.restoreService->LoadTabsFromLastSession();
  // A WebStateList batch operation can result in batch changes to the
  // TabRestoreService (e.g., closing or restoring all tabs). To properly batch
  // process TabRestoreService changes, those changes must be executed after the
  // WebStateList batch operation ended. This allows RecentTabs to ignore
  // individual tabRestoreServiceChanged calls that correspond to a WebStateList
  // batch operation. The consumer is updated once after all batch operations
  // have completed.
  if (_webStateListsWithBatchOperations.empty()) {
    [self.consumer refreshRecentlyClosedTabs];
  }
}

- (void)tabRestoreServiceDestroyed:(sessions::TabRestoreService*)service {
  [self.consumer setTabRestoreService:nullptr];
}

#pragma mark - WebStateListObserving

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  _webStateListsWithBatchOperations.insert(webStateList);
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  _webStateListsWithBatchOperations.erase(webStateList);
  // A WebStateList batch operation can result in batch changes to the
  // TabRestoreService (e.g., closing or restoring all tabs). Individual
  // TabRestoreService updates are ignored between
  // `-webStateListWillBeginBatchOperation:` and
  // `-webStateListBatchOperationEnded:` for all observed WebStateLists. The
  // consumer is updated once after all batch operations have completed.
  if (_webStateListsWithBatchOperations.empty()) {
    [self.consumer refreshRecentlyClosedTabs];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  if (_webStateListsWithBatchOperations.contains(webStateList)) {
    // This means a WebStateList was in a batch operation (received
    // `-webStateListWillBeginBatchOperation:`) that didn't finish (didn't
    // receive `-webStateListBatchOperationEnded:`). This is not supposed to
    // happen, but if it did, handle it by removing the web state list from the
    // set and dump without crashing.
    base::debug::DumpWithoutCrashing();
    _webStateListsWithBatchOperations.erase(webStateList);
    if (_webStateListsWithBatchOperations.empty()) {
      [self.consumer refreshRecentlyClosedTabs];
    }
  }
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

#pragma mark - Private

// Returns whether this profile has any foreign sessions to sync.
- (SessionsSyncUserState)userSignedInState {
  const auto requiredConsent =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  if (!_identityManager->HasPrimaryAccount(requiredConsent)) {
    // This returns "signed out" when the user is signed-in non-syncing and
    // kReplaceSyncPromosWithSignInPromos is off. That's a pre-existing issue.
    return SessionsSyncUserState::USER_SIGNED_OUT;
  }

  if (UserActionIsRequiredToHaveTabSyncWork(_syncService)) {
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF;
  }

  DCHECK(self.sessionSyncService);
  sync_sessions::OpenTabsUIDelegate* delegate =
      self.sessionSyncService->GetOpenTabsUIDelegate();
  if (!delegate) {
    return SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS;
  }

  std::vector<const sync_sessions::SyncedSession*> sessions;
  return delegate->GetAllForeignSessions(&sessions)
             ? SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS
             : SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS;
}

// Creates and send a tab grid toolbar configuration with button that should be
// displayed when recent grid is selected.
- (void)configureToolbarsButtons {
  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] init];
  toolbarsConfiguration.doneButton = YES;
  toolbarsConfiguration.searchButton = YES;
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

#pragma mark - RecentTabsTableViewControllerDelegate

- (void)refreshSessionsView {
  // This method is called from three places: 1) when this mediator observes a
  // change in the synced session state,  2) when the UI layer recognizes
  // that the signin process has completed, and 3) when the history & tabs sync
  // opt-in screen is dismissed.
  // The 2 latter calls are necessary because they can happen much more
  // immediately than the former call.
  [self.consumer refreshUserState:[self userSignedInState]];
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectRemotePanel"));
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

    [self configureToolbarsButtons];
    [self.gridConsumer setItemsCanBeRestored:NO];
    [self.gridConsumer setItemsCanBeClosed:NO];
  }
  // TODO(crbug.com/1457146): Implement.
}

@end
