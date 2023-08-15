// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/media/quick_settings_media_view_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "media/base/media_switches.h"

namespace ash {

class QuickSettingsMediaViewTest : public NoSessionAshTestBase {
 public:
  QuickSettingsMediaViewTest() = default;
  QuickSettingsMediaViewTest(const QuickSettingsMediaViewTest&) = delete;
  QuickSettingsMediaViewTest& operator=(const QuickSettingsMediaViewTest&) =
      delete;
  ~QuickSettingsMediaViewTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, media::kGlobalMediaControlsCrOSUpdatedUI}, {});
    NoSessionAshTestBase::SetUp();

    MediaTray::SetPinnedToShelf(false);
    GetPrimaryUnifiedSystemTray()->ShowBubble();
    item_ = std::make_unique<testing::NiceMock<
        media_message_center::test::MockMediaNotificationItem>>();
  }

  QuickSettingsMediaView* view() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller()
        ->media_view_controller()
        ->media_view_for_testing();
  }

  base::WeakPtr<media_message_center::test::MockMediaNotificationItem> item() {
    return item_->GetWeakPtr();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<media_message_center::test::MockMediaNotificationItem> item_;
};

TEST_F(QuickSettingsMediaViewTest, ShowOrHideItem) {
  const std::string item_id = "item_id";
  std::unique_ptr<global_media_controls::MediaItemUIView> item_ui =
      std::make_unique<global_media_controls::MediaItemUIView>(
          item_id, item(), nullptr, nullptr);

  EXPECT_EQ(0, static_cast<int>(view()->items_for_testing().size()));
  EXPECT_EQ(-1, view()->pagination_model_for_testing()->total_pages());

  view()->ShowItem(item_id, std::move(item_ui));
  EXPECT_EQ(1, static_cast<int>(view()->items_for_testing().size()));
  EXPECT_TRUE(view()->items_for_testing().contains(item_id));
  EXPECT_EQ(1, view()->pagination_model_for_testing()->total_pages());

  view()->HideItem(item_id);
  EXPECT_EQ(0, static_cast<int>(view()->items_for_testing().size()));
  EXPECT_EQ(0, view()->pagination_model_for_testing()->total_pages());
}

}  // namespace ash
