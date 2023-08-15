// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Pixel tests for `GlanceablesTaskView` that covers all possible permutations
// of UI states.
class GlanceablesTaskViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    base::Time due_date;
    ASSERT_TRUE(base::Time::FromString("2022-12-21T23:38:22.590Z", &due_date));

    task_ = std::make_unique<GlanceablesTask>(
        "task-id", "Task title",
        /*completed=*/false,
        has_due_date() ? absl::make_optional(due_date) : absl::nullopt,
        has_subtasks(),
        /*has_email_link=*/false,
        /*has_notes=*/has_notes());

    widget_ = CreateFramelessTestWidget();
    widget_->SetBounds(gfx::Rect(/*width=*/370, /*height=*/50));
    widget_->SetContentsView(std::make_unique<GlanceablesTaskView>(
        /*task_list_id=*/"task-list-id", task_.get()));
  }

  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = use_rtl();
    return init_params;
  }

  std::string GenerateScreenshotName() const {
    std::vector<std::string> parameters = {
        use_rtl() ? "rtl" : "ltr",
        has_due_date() ? "has_due_date=true" : "has_due_date=false",
        has_subtasks() ? "has_subtasks=true" : "has_subtasks=false",
        has_notes() ? "has_notes=true" : "has_notes=false"};

    std::string stringified_params = base::JoinString(parameters, "|");
    return base::JoinString({"glanceables_task_view", stringified_params}, ".");
  }

  views::Widget* widget() const { return widget_.get(); }

 private:
  bool use_rtl() const { return std::get<0>(GetParam()); }
  bool has_due_date() const { return std::get<1>(GetParam()); }
  bool has_subtasks() const { return std::get<2>(GetParam()); }
  bool has_notes() const { return std::get<3>(GetParam()); }

  base::test::ScopedFeatureList feature_list_{chromeos::features::kJelly};
  std::unique_ptr<GlanceablesTask> task_;
  std::unique_ptr<views::Widget> widget_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         GlanceablesTaskViewPixelTest,
                         testing::Combine(
                             /*use_rtl=*/testing::Bool(),
                             /*has_due_date=*/testing::Bool(),
                             /*has_subtasks=*/testing::Bool(),
                             /*has_notes=*/testing::Bool()));

TEST_P(GlanceablesTaskViewPixelTest, GlanceablesTaskView) {
  ASSERT_TRUE(widget());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(), /*revision_number=*/0, widget()));
}

}  // namespace ash
