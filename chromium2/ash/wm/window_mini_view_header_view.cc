// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view_header_view.h"

#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_mini_view.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The font delta of the window title.
constexpr int kLabelFontDelta = 2;

// Padding between header items.
constexpr int kHeaderPaddingDp = 12;
constexpr int kHeaderPaddingDpCrOSNext = 8;

// The corner radius for the top corners for the header.
constexpr int kHeaderTopCornerRadius = 16;

// The size in dp of the window icon shown on the alt-tab/overview window next
// to the title.
constexpr gfx::Size kIconSize = gfx::Size(24, 24);

constexpr gfx::Insets kHeaderInsets = gfx::Insets::TLBR(0, 10, 0, 10);

std::u16string GetWindowTitle(aura::Window* window) {
  aura::Window* transient_root = wm::GetTransientRoot(window);
  const std::u16string* overview_title =
      transient_root->GetProperty(chromeos::kWindowOverviewTitleKey);
  return (overview_title && !overview_title->empty())
             ? *overview_title
             : transient_root->GetTitle();
}

gfx::RoundedCornersF GetHeaderRoundedCorners(aura::Window* window) {
  const float scale = window->layer()->GetTargetTransform().To2dScale().x();
  const float scaled_corner_radius = kHeaderTopCornerRadius / scale;
  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  if (snap_group_controller) {
    SnapGroup* snap_group =
        snap_group_controller->GetSnapGroupForGivenWindow(window);
    if (snap_group) {
      auto* window1 = snap_group->window1();
      auto* window2 = snap_group->window2();
      CHECK(window == window1 || window == window2);
      // `window1` is guaranteed to be the primary snapped window in a snap
      // group and `window2` is guaranteed to be the secondary snapped window in
      // a snap group.
      return window == window1
                 ? gfx::RoundedCornersF(scaled_corner_radius, 0, 0, 0)
                 : gfx::RoundedCornersF(0, scaled_corner_radius, 0, 0);
    }
  }
  return gfx::RoundedCornersF(scaled_corner_radius, scaled_corner_radius, 0, 0);
}

}  // namespace

WindowMiniViewHeaderView::~WindowMiniViewHeaderView() = default;

WindowMiniViewHeaderView::WindowMiniViewHeaderView(
    WindowMiniView* window_mini_view)
    : window_mini_view_(window_mini_view) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();

  icon_label_view_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  icon_label_view_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  icon_label_view_->SetInsideBorderInsets(is_jellyroll_enabled ? kHeaderInsets
                                                               : gfx::Insets());
  icon_label_view_->SetBetweenChildSpacing(
      is_jellyroll_enabled ? kHeaderPaddingDpCrOSNext : kHeaderPaddingDp);

  title_label_ = icon_label_view_->AddChildView(std::make_unique<views::Label>(
      GetWindowTitle(window_mini_view_->source_window())));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  title_label_->SetEnabledColorId(
      is_jellyroll_enabled
          ? cros_tokens::kCrosSysPrimary
          : static_cast<ui::ColorId>(kColorAshTextColorPrimary));
  icon_label_view_->SetFlexForView(title_label_, 1);

  if (is_jellyroll_enabled) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        chromeos::features::IsJellyrollEnabled()
            ? cros_tokens::kCrosSysHeader
            : static_cast<ui::ColorId>(kColorAshShieldAndBase80),
        GetHeaderRoundedCorners(window_mini_view_->source_window()),
        /*for_border_thickness=*/0));

    views::Separator* separator =
        AddChildView(std::make_unique<views::Separator>());
    separator->SetColorId(kColorAshWindowHeaderStrokeColor);
  }
  SetFlexForView(icon_label_view_, 1);
}

void WindowMiniViewHeaderView::UpdateIconView(aura::Window* window) {
  aura::Window* transient_root = wm::GetTransientRoot(window);
  // Prefer kAppIconKey over kWindowIconKey as the app icon is typically
  // larger.
  gfx::ImageSkia* icon = transient_root->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty()) {
    icon = transient_root->GetProperty(aura::client::kWindowIconKey);
  }
  if (!icon) {
    return;
  }

  if (!icon_view_) {
    icon_view_ = icon_label_view_->AddChildViewAt(
        std::make_unique<views::ImageView>(), 0);
  }

  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
}

void WindowMiniViewHeaderView::UpdateTitleLabel(aura::Window* window) {
  title_label_->SetText(GetWindowTitle(window));
}

BEGIN_METADATA(WindowMiniViewHeaderView, views::View)
END_METADATA

}  // namespace ash
