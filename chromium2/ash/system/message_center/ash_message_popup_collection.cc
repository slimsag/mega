// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_message_popup_collection.h"

#include <cstdint>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/message_center/fullscreen_notification_blocker.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_view_factory.h"
#include "ash/system/message_center/metrics_utils.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

const int kPopupMarginX = 8;

// If available space is below this limit, we will disable expand/collapse
// behavior on each popup. We choose this value because this is roughly the
// height of the largest expanded (non grouped) notification.
const int kMinimumHeightToEnableExpandCollapse = 327;

void ReportPopupAnimationSmoothness(int smoothness) {
  base::UmaHistogramPercentage("Ash.NotificationPopup.AnimationSmoothness",
                               smoothness);
}

// Checks if `message_view` is associated with a parent notification.
bool IsParentNotification(message_center::MessageView* message_view) {
  if (!message_view) {
    return false;
  }

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          message_view->notification_id());

  if (!notification) {
    return false;
  }

  return notification->group_parent();
}

}  // namespace

const char AshMessagePopupCollection::kMessagePopupWidgetName[] =
    "ash/message_center/MessagePopup";

AshMessagePopupCollection::AshMessagePopupCollection(Shelf* shelf)
    : screen_(nullptr), shelf_(shelf) {
  shelf_->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->system_tray_notifier()->AddSystemTrayObserver(this);
}

AshMessagePopupCollection::~AshMessagePopupCollection() {
  Shell::Get()->system_tray_notifier()->RemoveSystemTrayObserver(this);
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  shelf_->RemoveObserver(this);
  for (views::Widget* widget : tracked_widgets_)
    widget->RemoveObserver(this);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void AshMessagePopupCollection::StartObserving(
    display::Screen* screen,
    const display::Display& display) {
  screen_ = screen;
  work_area_ = display.work_area();
  display_observer_.emplace(this);
  if (baseline_offset_ > 0) {
    UpdateWorkArea();
  }
}

void AshMessagePopupCollection::SetBaselineOffset(int baseline_offset) {
  const int old_baseline_offset = baseline_offset_;

  baseline_offset_ = baseline_offset;

  DCHECK_GE(baseline_offset_, 0);
  if (baseline_offset_ != 0) {
    baseline_offset_ += message_center::kMarginBetweenPopups;
  }

  if (old_baseline_offset != baseline_offset_) {
    ResetBounds();
  }
}

int AshMessagePopupCollection::GetPopupOriginX(
    const gfx::Rect& popup_bounds) const {
  // Popups should always follow the status area and will usually show on the
  // bottom-right of the screen. They will show at the bottom-left whenever the
  // shelf is left-aligned or for RTL when the shelf is not right aligned.
  return ((base::i18n::IsRTL() && GetAlignment() != ShelfAlignment::kRight) ||
          IsFromLeft())
             ? work_area_.x() + kPopupMarginX
             : work_area_.right() - kPopupMarginX - popup_bounds.width();
}

int AshMessagePopupCollection::GetBaseline() const {
  gfx::Insets tray_bubble_insets = GetTrayBubbleInsets(shelf_->GetWindow());

  // `hotseat_widget()` might be null since it dtor-ed before this class.
  int hotseat_height =
      shelf_->hotseat_widget() &&
              shelf_->hotseat_widget()->state() == HotseatState::kExtended
          ? shelf_->hotseat_widget()->GetHotseatSize()
          : 0;

  // Decrease baseline by `kShelfDisplayOffset` to compensate for the adjustment
  // of edges in `Shelf::GetSystemTrayAnchorRect()`.
  return work_area_.bottom() - tray_bubble_insets.bottom() - baseline_offset_ -
         hotseat_height - kShelfDisplayOffset;
}

gfx::Rect AshMessagePopupCollection::GetWorkArea() const {
  gfx::Rect work_area_without_tray_bubble = work_area_;
  work_area_without_tray_bubble.set_height(
      work_area_without_tray_bubble.height() - baseline_offset_);
  return work_area_without_tray_bubble;
}

bool AshMessagePopupCollection::IsTopDown() const {
  return false;
}

bool AshMessagePopupCollection::IsFromLeft() const {
  return GetAlignment() == ShelfAlignment::kLeft;
}

bool AshMessagePopupCollection::RecomputeAlignment(
    const display::Display& display) {
  // Nothing needs to be done.
  return false;
}

void AshMessagePopupCollection::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    views::Widget::InitParams* init_params) {
  init_params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  init_params->shadow_elevation = ::wm::kShadowElevationInactiveWindow;
  // On ash, popups go in `SettingBubbleContainer` together with other tray
  // bubbles, so the most recent element on screen will appear in front.
  init_params->parent = shelf_->GetWindow()->GetRootWindow()->GetChildById(
      kShellWindowId_SettingBubbleContainer);

  // Make the widget activatable so it can receive focus when cycling through
  // windows (i.e. pressing ctrl + forward/back).
  init_params->activatable = views::Widget::InitParams::Activatable::kYes;
  init_params->name = kMessagePopupWidgetName;
  init_params->corner_radius = kMessagePopupCornerRadius;
  Shell::Get()->focus_cycler()->AddWidget(widget);
  widget->AddObserver(this);
  tracked_widgets_.insert(widget);
}

bool AshMessagePopupCollection::IsPrimaryDisplayForNotification() const {
  return screen_ &&
         GetCurrentDisplay().id() == screen_->GetPrimaryDisplay().id();
}

bool AshMessagePopupCollection::BlockForMixedFullscreen(
    const message_center::Notification& notification) const {
  return FullscreenNotificationBlocker::BlockForMixedFullscreen(
      notification, RootWindowController::ForWindow(shelf_->GetWindow())
                        ->IsInFullscreenMode());
}

void AshMessagePopupCollection::NotifyPopupAdded(
    message_center::MessagePopupView* popup) {
  MessagePopupCollection::NotifyPopupAdded(popup);
  popup->message_view()->AddObserver(this);
  metrics_utils::LogPopupShown(popup->message_view()->notification_id());
  last_pop_up_added_ = popup;
}

void AshMessagePopupCollection::NotifyPopupClosed(
    message_center::MessagePopupView* popup) {
  metrics_utils::LogPopupClosed(popup);
  MessagePopupCollection::NotifyPopupClosed(popup);
  popup->message_view()->RemoveObserver(this);
  if (last_pop_up_added_ == popup)
    last_pop_up_added_ = nullptr;
}

void AshMessagePopupCollection::NotifyPopupCollectionHeightChanged() {
  if (!features::IsNotifierCollisionEnabled()) {
    return;
  }

  AdjustBaselineBasedOnShelfPodBubble(/*triggered_by_bubble_change=*/false);
}

void AshMessagePopupCollection::AnimationStarted() {
  if (popups_animating_ == 0 && last_pop_up_added_) {
    // Since all the popup widgets use the same compositor, we only need to set
    // this when the first popup shows in the animation sequence.
    animation_tracker_.emplace(last_pop_up_added_->GetWidget()
                                   ->GetCompositor()
                                   ->RequestNewThroughputTracker());
    animation_tracker_->Start(metrics_util::ForSmoothness(
        base::BindRepeating(&ReportPopupAnimationSmoothness)));
  }
  ++popups_animating_;
}

void AshMessagePopupCollection::AnimationFinished() {
  --popups_animating_;
  if (!popups_animating_) {
    // Stop tracking when all animations are finished.
    if (animation_tracker_) {
      animation_tracker_->Stop();
      animation_tracker_.reset();
    }

    if (animation_idle_closure_) {
      std::move(animation_idle_closure_).Run();
    }
  }
}

message_center::MessagePopupView* AshMessagePopupCollection::CreatePopup(
    const message_center::Notification& notification) {
  bool a11_feedback_on_init =
      notification.rich_notification_data()
          .should_make_spoken_feedback_for_popup_updates;
  return new message_center::MessagePopupView(
      MessageViewFactory::Create(notification, /*shown_in_popup=*/true)
          .release(),
      this, a11_feedback_on_init);
}

void AshMessagePopupCollection::ClosePopupItem(const PopupItem& item) {
  // We lock closing tray bubble here to prevent a bubble close when popup item
  // is removed (b/291988617).
  auto lock = TrayBackgroundView::DisableCloseBubbleOnWindowActivated();

  message_center::MessagePopupCollection::ClosePopupItem(item);
}

void AshMessagePopupCollection::OnTabletModeStarted() {
  // Reset bounds so pop-up baseline is updated.
  ResetBounds();
}

void AshMessagePopupCollection::OnTabletModeEnded() {
  // Reset bounds so pop-up baseline is updated.
  ResetBounds();
}

void AshMessagePopupCollection::OnStatusAreaAnchoredBubbleVisibilityChanged(
    TrayBubbleView* tray_bubble,
    bool visible) {
  AdjustBaselineBasedOnBubbleChange(tray_bubble, /*bubble_visible=*/visible);
}

void AshMessagePopupCollection::OnTrayBubbleBoundsChanged(
    TrayBubbleView* tray_bubble) {
  AdjustBaselineBasedOnBubbleChange(tray_bubble, /*bubble_visible=*/true);
}

bool AshMessagePopupCollection::IsWidgetAPopupNotification(
    views::Widget* widget) {
  for (auto* popup_widget : tracked_widgets_) {
    if (widget == popup_widget) {
      return true;
    }
  }
  return false;
}

void AshMessagePopupCollection::SetAnimationIdleClosureForTest(
    base::OnceClosure closure) {
  DCHECK(closure);
  DCHECK(!animation_idle_closure_);
  animation_idle_closure_ = std::move(closure);
}

void AshMessagePopupCollection::OnSlideOut(const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/true,
                                 /*is_popup=*/true);
}

void AshMessagePopupCollection::OnCloseButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/false,
                                 /*is_popup=*/true);
}

void AshMessagePopupCollection::OnSettingsButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSettingsShown(notification_id, /*is_slide_controls=*/false,
                                  /*is_popup=*/true);
}

void AshMessagePopupCollection::OnSnoozeButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSnoozed(notification_id, /*is_slide_controls=*/false,
                            /*is_popup=*/true);
}

ShelfAlignment AshMessagePopupCollection::GetAlignment() const {
  return shelf_->alignment();
}

display::Display AshMessagePopupCollection::GetCurrentDisplay() const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      shelf_->GetWindow());
}

void AshMessagePopupCollection::UpdateWorkArea() {
  gfx::Rect new_work_area =
      WorkAreaInsets::ForWindow(shelf_->GetWindow()->GetRootWindow())
          ->user_work_area_bounds();
  if (work_area_ == new_work_area)
    return;

  work_area_ = new_work_area;
  ResetBounds();
}

void AshMessagePopupCollection::AdjustBaselineBasedOnBubbleChange(
    TrayBubbleView* tray_bubble,
    bool bubble_visible) {
  if (!features::IsNotifierCollisionEnabled()) {
    return;
  }

  if (tray_bubble && tray_bubble->GetBubbleType() ==
                         TrayBubbleView::TrayBubbleType::kSecondaryBubble) {
    AdjustBaselineBasedOnSecondaryBubble(tray_bubble, bubble_visible);
    return;
  }

  AdjustBaselineBasedOnShelfPodBubble(/*triggered_by_bubble_change=*/true);
}

void AshMessagePopupCollection::AdjustBaselineBasedOnShelfPodBubble(
    bool triggered_by_bubble_change) {
  CHECK(features::IsNotifierCollisionEnabled());

  auto* status_area = StatusAreaWidget::ForWindow(shelf_->GetWindow());
  auto* shelf_pod_bubble =
      status_area ? status_area->open_shelf_pod_bubble() : nullptr;

  // The tray bubble might already be closed/deleted. We also only put the popup
  // on top of tray bubble that is anchored to the shelf corner.
  if (!shelf_pod_bubble || !shelf_pod_bubble->IsAnchoredToShelfCorner()) {
    SetBaselineOffset(0);

    // Note that `available_space_above_popups` is not used when
    // `shelf_bubble_open` is false.
    UpdateExpandCollapseEnabledForPopups(/*shelf_bubble_open=*/false,
                                         /*available_space_above_popups=*/0);
    return;
  }

  // The space left on the screen above the popups. If the popups collection
  // cannot fit into the top of the bubble, this value will be negative.
  int available_space_above_popups = shelf_pod_bubble->GetBoundsInScreen().y() -
                                     message_center::kMarginBetweenPopups -
                                     popup_collection_bounds().height();

  UpdateExpandCollapseEnabledForPopups(/*shelf_bubble_open=*/true,
                                       available_space_above_popups);

  // If there's not enough space above the tray bubble to display the entire
  // popup collection, we will close the popups if possible. Otherwise, we will
  // just display the popup on top of the tray bubble (adjust the baseline back
  // to zero and move down the popups).
  if (available_space_above_popups < 0) {
    // We want to avoid showing tray bubble and popups overlapping with each
    // other. Thus, when this function is triggered by a change that happens in
    // the bubble (bubble size or visibility changed), we will close the popup.
    if (triggered_by_bubble_change) {
      CloseAllPopupsNow();
      ResetBounds();
    } else {
      SetBaselineOffset(0);
      MoveDownPopups();
    }
    return;
  }

  SetBaselineOffset(shelf_pod_bubble->height());
}

void AshMessagePopupCollection::AdjustBaselineBasedOnSecondaryBubble(
    TrayBubbleView* tray_bubble,
    bool visible) {
  CHECK(features::IsNotifierCollisionEnabled());

  DCHECK(tray_bubble);
  DCHECK_EQ(tray_bubble->GetBubbleType(),
            TrayBubbleView::TrayBubbleType::kSecondaryBubble);

  auto* status_area = StatusAreaWidget::ForWindow(shelf_->GetWindow());
  auto* current_open_shelf_pod_bubble =
      status_area ? status_area->open_shelf_pod_bubble() : nullptr;

  // If there's a current open shelf pod bubble, the popup should be on top of
  // that bubble, not on top of the secondary bubble, so do nothing here.
  if (current_open_shelf_pod_bubble &&
      current_open_shelf_pod_bubble != tray_bubble) {
    return;
  }

  SetBaselineOffset(visible ? tray_bubble->height() : 0);
}

///////////////////////////////////////////////////////////////////////////////
// ShelfObserver:

void AshMessagePopupCollection::OnShelfWorkAreaInsetsChanged() {
  UpdateWorkArea();
}

void AshMessagePopupCollection::OnHotseatStateChanged(HotseatState old_state,
                                                      HotseatState new_state) {
  ResetBounds();
}

///////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver:

void AshMessagePopupCollection::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (GetCurrentDisplay().id() == display.id())
    UpdateWorkArea();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver:

void AshMessagePopupCollection::OnWidgetClosing(views::Widget* widget) {
  Shell::Get()->focus_cycler()->RemoveWidget(widget);
  widget->RemoveObserver(this);
  tracked_widgets_.erase(widget);
}

void AshMessagePopupCollection::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  // Note: Each pop-up is contained in it's own widget and we need to manually
  // focus the contained MessageView when the widget is activated through the
  // FocusCycler.
  if (active && Shell::Get()->focus_cycler()->widget_activating() == widget)
    widget->GetFocusManager()->SetFocusedView(widget->GetContentsView());
}

void AshMessagePopupCollection::UpdateExpandCollapseEnabledForPopups(
    bool shelf_bubble_open,
    int available_space_above_popups) {
  for (const auto& item : popup_items()) {
    auto* message_view = item.popup->message_view();
    if (!message_view) {
      continue;
    }

    if (!shelf_bubble_open) {
      message_view->SetExpandCollapseEnabled(true);
      continue;
    }

    // If the space left on the screen above the popups is less than the
    // threshold, we will disable expand/collapse on all the popups. Also we
    // disable expand/collapse for all group notifications.
    message_view->SetExpandCollapseEnabled(
        IsParentNotification(message_view)
            ? false
            : available_space_above_popups >
                  kMinimumHeightToEnableExpandCollapse);
  }
}

}  // namespace ash
