// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/fake_glanceables_tasks_client.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

class TestNewWindowDelegateImpl : public TestNewWindowDelegate {
 public:
  // TestNewWindowDelegate:
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override {
    last_opened_url_ = url;
  }

  GURL last_opened_url() const { return last_opened_url_; }

 private:
  GURL last_opened_url_;
};

}  // namespace

class TasksBubbleViewTest : public AshTestBase {
 public:
  TasksBubbleViewTest() {
    auto new_window_delegate = std::make_unique<TestNewWindowDelegateImpl>();
    new_window_delegate_ = new_window_delegate.get();
    new_window_delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(
            std::move(new_window_delegate));
  }

  void SetUp() override {
    AshTestBase::SetUp();
    SimulateUserLogin(account_id_);
    fake_glanceables_tasks_client_ =
        std::make_unique<FakeGlanceablesTasksClient>(base::Time::Now());
    Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesV2Controller::ClientsRegistration{
                         .tasks_client = fake_glanceables_tasks_client_.get()});
    ASSERT_TRUE(Shell::Get()->glanceables_v2_controller()->GetTasksClient());

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    view_ = widget_->SetContentsView(
        std::make_unique<TasksBubbleView>(&detailed_view_delegate_));
  }

  void TearDown() override {
    // Destroy `widget_` first, before destroying `LayoutProvider` (needed in
    // the `views::Combobox`'s destruction chain).
    view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  views::Combobox* GetComboBoxView() const {
    return views::AsViewClass<views::Combobox>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleComboBox)));
  }

  bool IsMenuRunning() const {
    const auto* const combo_box = GetComboBoxView();
    return combo_box && combo_box->IsMenuRunning();
  }

  const views::View* GetTaskItemsContainerView() const {
    return views::AsViewClass<views::View>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleListContainer)));
  }

  const views::LabelButton* GetAddNewTaskButton() const {
    return views::AsViewClass<views::LabelButton>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleAddNewButton)));
  }

  const GlanceablesListFooterView* GetListFooterView() const {
    return views::AsViewClass<GlanceablesListFooterView>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kTasksBubbleListFooter)));
  }

  const views::ProgressBar* GetProgressBar() const {
    return views::AsViewClass<views::ProgressBar>(view_->GetViewByID(
        base::to_underlying(GlanceablesViewId::kProgressBar)));
  }

  FakeGlanceablesTasksClient* tasks_client() const {
    return fake_glanceables_tasks_client_.get();
  }

  const TestNewWindowDelegateImpl* new_window_delegate() const {
    return new_window_delegate_;
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  AccountId account_id_ = AccountId::FromUserEmail("test_user@gmail.com");
  std::unique_ptr<FakeGlanceablesTasksClient> fake_glanceables_tasks_client_;
  std::unique_ptr<ash::TestNewWindowDelegateProvider>
      new_window_delegate_provider_;
  raw_ptr<TestNewWindowDelegateImpl> new_window_delegate_;
  DetailedViewDelegate detailed_view_delegate_{nullptr};
  raw_ptr<TasksBubbleView> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(TasksBubbleViewTest, ShowTasksComboModel) {
  EXPECT_FALSE(IsMenuRunning());
  EXPECT_TRUE(GetComboBoxView()->GetVisible());

  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 2u);

  // Verify that tapping on combobox opens the selection menu.
  GestureTapOn(GetComboBoxView());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsMenuRunning());

  // Select the next task list using keyboard navigation.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

  // Verify the number of items in task_items_container_view()->children().
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 3u);
}

TEST_F(TasksBubbleViewTest, MarkTaskAsComplete) {
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 2u);

  auto* const task_view = views::AsViewClass<GlanceablesTaskView>(
      GetTaskItemsContainerView()->children()[0]);
  ASSERT_TRUE(task_view);
  ASSERT_FALSE(task_view->GetCompletedForTest());
  ASSERT_TRUE(tasks_client()->completed_tasks().empty());

  GestureTapOn(task_view->GetButtonForTest());
  ASSERT_TRUE(task_view->GetCompletedForTest());
  ASSERT_EQ(tasks_client()->completed_tasks().size(), 1u);
  EXPECT_EQ(tasks_client()->completed_tasks().front(),
            "TaskListID1:TaskListItem1");
}

TEST_F(TasksBubbleViewTest, ShowTasksWebUI) {
  const auto* const see_all_button =
      views::AsViewClass<views::LabelButton>(GetListFooterView()->GetViewByID(
          base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton)));
  GestureTapOn(see_all_button);
  EXPECT_EQ(new_window_delegate()->last_opened_url(),
            "https://calendar.google.com/calendar/u/0/r/week?opentasks=1");
}

TEST_F(TasksBubbleViewTest, ShowsAndHidesAddNewButton) {
  // Shows items from the first / default task list.
  EXPECT_TRUE(GetTaskItemsContainerView()->GetVisible());
  EXPECT_EQ(GetTaskItemsContainerView()->children().size(), 2u);
  EXPECT_FALSE(GetAddNewTaskButton()->GetVisible());
  EXPECT_TRUE(GetListFooterView()->GetVisible());

  // Switch to the empty task list.
  ASSERT_EQ(GetComboBoxView()->GetTextForRow(2), u"Task List 3 Title (empty)");
  GetComboBoxView()->MenuSelectionAt(2);
  EXPECT_FALSE(GetTaskItemsContainerView()->GetVisible());
  EXPECT_TRUE(GetTaskItemsContainerView()->children().empty());
  EXPECT_TRUE(GetAddNewTaskButton()->GetVisible());
  EXPECT_FALSE(GetListFooterView()->GetVisible());
}

TEST_F(TasksBubbleViewTest, ShowsProgressBarWhileLoadingTasks) {
  ASSERT_TRUE(GetProgressBar());
  ASSERT_TRUE(GetComboBoxView());

  tasks_client()->set_paused(true);

  // Initially progress bar is hidden.
  EXPECT_FALSE(GetProgressBar()->GetVisible());

  // Switch to another task list, the progress bar should become visible.
  GetComboBoxView()->MenuSelectionAt(2);
  EXPECT_TRUE(GetProgressBar()->GetVisible());

  // After replying to pending callbacks, the progress bar should become hidden.
  EXPECT_EQ(tasks_client()->RunPendingGetTasksCallbacks(), 1u);
  EXPECT_FALSE(GetProgressBar()->GetVisible());
}

}  // namespace ash
