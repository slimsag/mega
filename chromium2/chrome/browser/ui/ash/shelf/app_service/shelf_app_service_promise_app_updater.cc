// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/shelf_app_service_promise_app_updater.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/profiles/profile.h"

ShelfPromiseAppUpdater::ShelfPromiseAppUpdater(Delegate* delegate,
                                               Profile* profile)
    : ShelfAppUpdater(delegate, profile) {
  promise_app_registry_cache_observation_.Observe(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->PromiseAppRegistryCache());
}

ShelfPromiseAppUpdater::~ShelfPromiseAppUpdater() = default;

// PromiseAppRegistryCache::Observer overrides:
void ShelfPromiseAppUpdater::OnPromiseAppUpdate(
    const apps::PromiseAppUpdate& update) {
  // Trigger the Shelf item replacement if the promise app has been deleted.
  if (update.Status() == apps::PromiseStatus::kRemove) {
    delegate()->OnPromiseAppRemoved(update.PackageId());
  }

  // We should only make changes to the Shelf Item if the promise app needs to
  // be shown.
  if (!update.ShouldShow()) {
    return;
  }
  delegate()->OnPromiseAppUpdate(update);
}

void ShelfPromiseAppUpdater::OnPromiseAppRegistryCacheWillBeDestroyed(
    apps::PromiseAppRegistryCache* cache) {
  promise_app_registry_cache_observation_.Reset();
}
