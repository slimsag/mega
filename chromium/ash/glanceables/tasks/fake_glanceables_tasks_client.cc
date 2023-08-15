// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/fake_glanceables_tasks_client.h"

#include <list>
#include <utility>

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_util.h"

namespace ash {

FakeGlanceablesTasksClient::FakeGlanceablesTasksClient(
    base::Time tasks_due_time) {
  PopulateTasks(tasks_due_time);
  PopulateTaskLists(tasks_due_time);
}

FakeGlanceablesTasksClient::~FakeGlanceablesTasksClient() = default;

void FakeGlanceablesTasksClient::GetTaskLists(GetTaskListsCallback callback) {
  std::move(callback).Run(task_lists_.get());
}

void FakeGlanceablesTasksClient::GetTasks(const std::string& task_list_id,
                                          GetTasksCallback callback) {
  auto iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(iter != tasks_in_task_lists_.end());

  if (!paused_) {
    std::move(callback).Run(iter->second.get());
  } else {
    pending_get_tasks_callbacks_.push_back(base::BindOnce(
        [](ui::ListModel<ash::GlanceablesTask>* tasks,
           GetTasksCallback callback) { std::move(callback).Run(tasks); },
        iter->second.get(), std::move(callback)));
  }
}

void FakeGlanceablesTasksClient::MarkAsCompleted(
    const std::string& task_list_id,
    const std::string& task_id,
    MarkAsCompletedCallback callback) {
  completed_tasks_.push_back(base::JoinString({task_list_id, task_id}, ":"));
  std::move(callback).Run(/*success=*/true);
}

void FakeGlanceablesTasksClient::OnGlanceablesBubbleClosed() {
  ++bubble_closed_count_;
}

int FakeGlanceablesTasksClient::GetAndResetBubbleClosedCount() {
  int result = bubble_closed_count_;
  bubble_closed_count_ = 0;
  return result;
}

size_t FakeGlanceablesTasksClient::RunPendingGetTasksCallbacks() {
  std::list<base::OnceClosure> callbacks;
  pending_get_tasks_callbacks_.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
  return callbacks.size();
}

void FakeGlanceablesTasksClient::PopulateTasks(base::Time tasks_due_time) {
  task_lists_ = std::make_unique<ui::ListModel<GlanceablesTaskList>>();

  task_lists_->Add(std::make_unique<GlanceablesTaskList>(
      "TaskListID1", "Task List 1 Title", /*updated=*/tasks_due_time));
  task_lists_->Add(std::make_unique<GlanceablesTaskList>(
      "TaskListID2", "Task List 2 Title", /*updated=*/tasks_due_time));
  task_lists_->Add(std::make_unique<GlanceablesTaskList>(
      "TaskListID3", "Task List 3 Title (empty)",
      /*updated=*/tasks_due_time));
}

void FakeGlanceablesTasksClient::PopulateTaskLists(base::Time tasks_due_time) {
  std::unique_ptr<ui::ListModel<GlanceablesTask>> task_list_1 =
      std::make_unique<ui::ListModel<GlanceablesTask>>();
  task_list_1->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem1", "Task List 1 Item 1 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  task_list_1->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem2", "Task List 1 Item 2 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  std::unique_ptr<ui::ListModel<GlanceablesTask>> task_list_2 =
      std::make_unique<ui::ListModel<GlanceablesTask>>();
  task_list_2->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem3", "Task List 2 Item 1 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  task_list_2->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem4", "Task List 2 Item 2 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  task_list_2->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem5", "Task List 2 Item 3 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  tasks_in_task_lists_.emplace("TaskListID1", std::move(task_list_1));
  tasks_in_task_lists_.emplace("TaskListID2", std::move(task_list_2));
  tasks_in_task_lists_.emplace(
      "TaskListID3", std::make_unique<ui::ListModel<GlanceablesTask>>());
}

}  // namespace ash
