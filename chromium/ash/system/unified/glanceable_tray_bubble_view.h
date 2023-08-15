// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_

#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/weak_ptr.h"

namespace ash {
class CalendarView;
class ClassroomBubbleStudentView;
class ClassroomBubbleTeacherView;
class DetailedViewDelegate;
class TasksBubbleView;
class Shelf;

// The bubble associated with the `GlanceableTrayBubble`. This bubble is the
// container for the child `tasks` and `classroom` glanceables.
class GlanceableTrayBubbleView : public TrayBubbleView {
 public:
  GlanceableTrayBubbleView(const InitParams& init_params, Shelf* shelf);
  GlanceableTrayBubbleView(const GlanceableTrayBubbleView&) = delete;
  GlanceableTrayBubbleView& operator=(const GlanceableTrayBubbleView&) = delete;
  ~GlanceableTrayBubbleView() override;

  void InitializeContents();

  TasksBubbleView* GetTasksView() { return tasks_bubble_view_; }
  ClassroomBubbleTeacherView* GetClassroomTeacherView() {
    return classroom_bubble_teacher_view_;
  }
  ClassroomBubbleStudentView* GetClassroomStudentView() {
    return classroom_bubble_student_view_;
  }
  CalendarView* GetCalendarView() { return calendar_view_; }

  // TrayBubbleView:
  bool CanActivate() const override;

 private:
  // Creates classroom student or teacher view if needed (if the corresponding
  // role is active) and stores the pointer in `view`.
  // NOTE: in the rare case, when a single user has both student and teacher
  // roles in different courses, the order of the two bubbles is not guaranteed.
  template <typename T>
  void AddClassroomBubbleViewIfNeeded(raw_ptr<T, ExperimentalAsh>* view,
                                      bool is_role_active);

  void OnGlanceablesContainerPreferredSizeChanged();
  void OnGlanceablesContainerHeightChanged(int height_delta);

  const raw_ptr<Shelf, ExperimentalAsh> shelf_;
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  // A scrollable view which contains the individual glanceables.
  raw_ptr<views::ScrollView, ExperimentalAsh> scroll_view_ = nullptr;

  // Child bubble view for the tasks glanceable. Owned by bubble_view_.
  raw_ptr<TasksBubbleView, ExperimentalAsh> tasks_bubble_view_ = nullptr;

  // Child bubble view for the teacher classrooms glanceable. Owned by
  // bubble_view_.
  raw_ptr<ClassroomBubbleTeacherView, ExperimentalAsh>
      classroom_bubble_teacher_view_ = nullptr;

  // Child bubble view for the student classrooms glanceable. Owned by
  // bubble_view_.
  raw_ptr<ClassroomBubbleStudentView, ExperimentalAsh>
      classroom_bubble_student_view_ = nullptr;

  // Child bubble view for the calendar glanceable. Owned by bubble_view_.
  raw_ptr<CalendarView, ExperimentalAsh> calendar_view_ = nullptr;

  base::CallbackListSubscription on_contents_scrolled_subscription_;

  base::WeakPtrFactory<GlanceableTrayBubbleView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_GLANCEABLE_TRAY_BUBBLE_VIEW_H_
