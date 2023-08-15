// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

const PackageId kTestPackageId(AppType::kArc, "test.package.name");

class PromiseAppRegistryCacheTest : public testing::Test {
 public:
  void SetUp() override {
    cache_ = std::make_unique<PromiseAppRegistryCache>();
  }

  PromiseAppRegistryCache* cache() { return cache_.get(); }

 private:
  std::unique_ptr<PromiseAppRegistryCache> cache_;
};

TEST_F(PromiseAppRegistryCacheTest, OnPromiseApp_AddsPromiseAppToCache) {
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  ASSERT_FALSE(cache()->HasPromiseApp(kTestPackageId));
  cache()->OnPromiseApp(std::move(promise_app));
  ASSERT_TRUE(cache()->HasPromiseApp(kTestPackageId));
}

TEST_F(PromiseAppRegistryCacheTest, OnPromiseApp_UpdatesPromiseAppProgress) {
  float progress_initial = 0.1;
  float progress_next = 0.9;

  // Check that there aren't any promise apps registered yet.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 0u);

  // Pre-register a promise app with no installation progress value.
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  cache()->OnPromiseApp(std::move(promise_app));
  EXPECT_FALSE(cache()->GetPromiseApp(kTestPackageId)->progress.has_value());
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 1u);

  // Update the progress value for the correct app and confirm the progress
  // value.
  auto promise_delta = std::make_unique<PromiseApp>(kTestPackageId);
  promise_delta->progress = progress_initial;
  cache()->OnPromiseApp(std::move(promise_delta));
  EXPECT_EQ(cache()->GetPromiseApp(kTestPackageId)->progress, progress_initial);

  // Update the progress value again and check if it is the correct value.
  auto promise_delta_next = std::make_unique<PromiseApp>(kTestPackageId);
  promise_delta_next->progress = progress_next;
  cache()->OnPromiseApp(std::move(promise_delta_next));
  EXPECT_EQ(cache()->GetPromiseApp(kTestPackageId)->progress, progress_next);

  // All these changes should have applied to the same promise app instead
  // of creating new ones.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 1u);
}

TEST_F(PromiseAppRegistryCacheTest, GetAllPromiseApps) {
  // There should be no promise apps registered yet.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 0u);

  // Register some promise apps.
  auto package_id_1 = PackageId(AppType::kArc, "test1");
  auto promise_app_1 = std::make_unique<PromiseApp>(package_id_1);
  cache()->OnPromiseApp(std::move(promise_app_1));

  auto package_id_2 = PackageId(AppType::kArc, "test2");
  auto promise_app_2 = std::make_unique<PromiseApp>(package_id_2);
  cache()->OnPromiseApp(std::move(promise_app_2));

  // Check that all the promise apps are being retrieved.
  auto promise_app_list = cache()->GetAllPromiseApps();
  EXPECT_EQ(promise_app_list.size(), 2u);
  EXPECT_EQ(promise_app_list[0]->package_id, package_id_1);
  EXPECT_EQ(promise_app_list[1]->package_id, package_id_2);
}

TEST_F(PromiseAppRegistryCacheTest, GetPromiseAppForStringPackageId) {
  // There should be no promise apps registered yet.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 0u);

  std::string valid_package_id_1 = "android:something.example.test";
  std::string valid_package_id_2 = "android:other.example.test";
  std::string invalid_package_id = "invalid";
  apps::PackageId package_id =
      PackageId::FromString(valid_package_id_1).value();

  // Register a promise app.
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  cache()->OnPromiseApp(std::move(promise_app));

  // Expect nullptr result for invalid string Package ID or when a Package ID
  // isn't registered.
  EXPECT_FALSE(cache()->GetPromiseAppForStringPackageId(invalid_package_id));
  EXPECT_FALSE(cache()->GetPromiseAppForStringPackageId(valid_package_id_2));

  const PromiseApp* promise_app_result =
      cache()->GetPromiseAppForStringPackageId(valid_package_id_1);
  EXPECT_EQ(promise_app_result->package_id, package_id);
}

TEST_F(PromiseAppRegistryCacheTest, RemovePromiseApp) {
  // Register a promise app.
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  cache()->OnPromiseApp(std::move(promise_app));

  // Confirm that the promise app is registered.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));

  // Update the promise app with a kRemove status.
  auto delta = std::make_unique<PromiseApp>(kTestPackageId);
  delta->status = PromiseStatus::kRemove;
  cache()->OnPromiseApp(std::move(delta));

  // Confirm that the promise app was removed.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));
}

class PromiseAppRegistryCacheObserverTest : public testing::Test,
                                            PromiseAppRegistryCache::Observer {
 public:
  void SetUp() override {
    cache_ = std::make_unique<PromiseAppRegistryCache>();
  }

  // apps::PromiseAppRegistryCache::Observer:
  void OnPromiseAppUpdate(const PromiseAppUpdate& update) override {
    EXPECT_EQ(update, *expected_update_);
    on_promise_app_updated_called_ = true;
  }

  void OnPromiseAppRegistryCacheWillBeDestroyed(
      apps::PromiseAppRegistryCache* cache) override {
    obs_.Reset();
  }

  void ExpectPromiseAppUpdate(std::unique_ptr<PromiseAppUpdate> update) {
    expected_update_ = std::move(update);
    if (!obs_.IsObserving()) {
      obs_.Observe(cache());
    }
    on_promise_app_updated_called_ = false;
  }

  bool CheckOnPromiseAppUpdatedCalled() {
    return on_promise_app_updated_called_;
  }

  PromiseAppRegistryCache* cache() { return cache_.get(); }

 private:
  base::ScopedObservation<PromiseAppRegistryCache,
                          PromiseAppRegistryCache::Observer>
      obs_{this};
  std::unique_ptr<PromiseAppUpdate> expected_update_;
  std::unique_ptr<PromiseAppRegistryCache> cache_;
  bool on_promise_app_updated_called_;
};

TEST_F(PromiseAppRegistryCacheObserverTest, OnPromiseAppUpdate_NewPromiseApp) {
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->name = "Test";
  promise_app->progress = 0;
  promise_app->status = PromiseStatus::kPending;
  promise_app->should_show = false;

  ASSERT_FALSE(cache()->HasPromiseApp(kTestPackageId));

  // Check that we get the appropriate update when registering a new promise
  // app.
  ExpectPromiseAppUpdate(
      std::make_unique<PromiseAppUpdate>(nullptr, promise_app.get()));
  cache()->OnPromiseApp(std::move(promise_app));
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
}

TEST_F(PromiseAppRegistryCacheObserverTest,
       OnPromiseAppUpdate_ModifyPromiseApp) {
  auto promise_app_pending = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_pending->status = PromiseStatus::kPending;
  promise_app_pending->should_show = false;
  ExpectPromiseAppUpdate(
      std::make_unique<PromiseAppUpdate>(nullptr, promise_app_pending.get()));
  cache()->OnPromiseApp(promise_app_pending->Clone());
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());

  // Check that we get the appropriate update when going from pending to
  // installing.
  auto promise_app_installing = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_installing->name = "Test";
  promise_app_installing->progress = 0.4;
  promise_app_installing->status = PromiseStatus::kInstalling;
  promise_app_installing->should_show = true;
  ExpectPromiseAppUpdate(std::make_unique<PromiseAppUpdate>(
      promise_app_pending.get(), promise_app_installing.get()));
  EXPECT_FALSE(CheckOnPromiseAppUpdatedCalled());
  cache()->OnPromiseApp(std::move(promise_app_installing));
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
}

}  // namespace apps
