// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/check.h"
#include "base/notreached.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

constexpr int kMouseRemappableFlags = ui::EF_BACK_MOUSE_BUTTON |
                                      ui::EF_FORWARD_MOUSE_BUTTON |
                                      ui::EF_MIDDLE_MOUSE_BUTTON;

constexpr int kGraphicsTabletRemappableFlags =
    ui::EF_RIGHT_MOUSE_BUTTON | ui::EF_BACK_MOUSE_BUTTON |
    ui::EF_FORWARD_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON;

bool IsMouseButtonEvent(const ui::MouseEvent& mouse_event) {
  return mouse_event.type() == ui::ET_MOUSE_PRESSED ||
         mouse_event.type() == ui::ET_MOUSE_RELEASED;
}

bool IsMouseRemappableButton(int flags) {
  return (flags & kMouseRemappableFlags) != 0;
}

bool IsGraphicsTabletRemappableButton(int flags) {
  return (flags & kGraphicsTabletRemappableFlags) != 0;
}

int GetRemappableMouseEventFlags(
    PeripheralCustomizationEventRewriter::DeviceType device_type) {
  switch (device_type) {
    case PeripheralCustomizationEventRewriter::DeviceType::kMouse:
      return kMouseRemappableFlags;
    case PeripheralCustomizationEventRewriter::DeviceType::kGraphicsTablet:
      return kGraphicsTabletRemappableFlags;
  }
}

mojom::ButtonPtr GetButtonFromMouseEventFlag(int flag) {
  switch (flag) {
    case ui::EF_LEFT_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kLeft);
    case ui::EF_RIGHT_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kRight);
    case ui::EF_MIDDLE_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kMiddle);
    case ui::EF_FORWARD_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kForward);
    case ui::EF_BACK_MOUSE_BUTTON:
      return mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kBack);
  }

  NOTREACHED_NORETURN();
}

}  // namespace

PeripheralCustomizationEventRewriter::PeripheralCustomizationEventRewriter() =
    default;
PeripheralCustomizationEventRewriter::~PeripheralCustomizationEventRewriter() =
    default;

absl::optional<PeripheralCustomizationEventRewriter::DeviceType>
PeripheralCustomizationEventRewriter::GetDeviceTypeToObserve(int device_id) {
  if (mice_to_observe_.contains(device_id)) {
    return DeviceType::kMouse;
  }
  if (graphics_tablets_to_observe_.contains(device_id)) {
    return DeviceType::kGraphicsTablet;
  }
  return absl::nullopt;
}

void PeripheralCustomizationEventRewriter::StartObservingMouse(int device_id) {
  mice_to_observe_.insert(device_id);
}

void PeripheralCustomizationEventRewriter::StartObservingGraphicsTablet(
    int device_id) {
  graphics_tablets_to_observe_.insert(device_id);
}

void PeripheralCustomizationEventRewriter::StopObserving() {
  graphics_tablets_to_observe_.clear();
  mice_to_observe_.clear();
}

bool PeripheralCustomizationEventRewriter::NotifyMouseEventObserving(
    const ui::MouseEvent& mouse_event,
    DeviceType device_type) {
  if (!IsMouseButtonEvent(mouse_event)) {
    return false;
  }

  // Make sure the button is remappable for the current `device_type`.
  switch (device_type) {
    case DeviceType::kMouse:
      if (!IsMouseRemappableButton(mouse_event.changed_button_flags())) {
        return false;
      }
      break;
    case DeviceType::kGraphicsTablet:
      if (!IsGraphicsTabletRemappableButton(
              mouse_event.changed_button_flags())) {
        return false;
      }
      break;
  }

  if (mouse_event.type() != ui::ET_MOUSE_PRESSED) {
    return true;
  }

  const auto button =
      GetButtonFromMouseEventFlag(mouse_event.changed_button_flags());
  for (auto& observer : observers_) {
    switch (device_type) {
      case DeviceType::kMouse:
        observer.OnMouseButtonPressed(mouse_event.source_device_id(), *button);
        break;
      case DeviceType::kGraphicsTablet:
        observer.OnGraphicsTabletButtonPressed(mouse_event.source_device_id(),
                                               *button);
        break;
    }
  }

  return true;
}

bool PeripheralCustomizationEventRewriter::NotifyKeyEventObserving(
    const ui::KeyEvent& key_event,
    DeviceType device_type) {
  // Observers should only be notified on key presses.
  if (key_event.type() != ui::ET_KEY_PRESSED) {
    return true;
  }

  const auto button = mojom::Button::NewVkey(key_event.key_code());
  for (auto& observer : observers_) {
    switch (device_type) {
      case DeviceType::kMouse:
        observer.OnMouseButtonPressed(key_event.source_device_id(), *button);
        break;
      case DeviceType::kGraphicsTablet:
        observer.OnGraphicsTabletButtonPressed(key_event.source_device_id(),
                                               *button);
        break;
    }
  }

  return true;
}

ui::EventDispatchDetails PeripheralCustomizationEventRewriter::RewriteKeyEvent(
    const ui::KeyEvent& key_event,
    const Continuation continuation) {
  auto device_type_to_observe =
      GetDeviceTypeToObserve(key_event.source_device_id());
  if (device_type_to_observe) {
    if (NotifyKeyEventObserving(key_event, *device_type_to_observe)) {
      return DiscardEvent(continuation);
    }
  }

  return SendEvent(continuation, &key_event);
}

ui::EventDispatchDetails
PeripheralCustomizationEventRewriter::RewriteMouseEvent(
    const ui::MouseEvent& mouse_event,
    const Continuation continuation) {
  auto device_type_to_observe =
      GetDeviceTypeToObserve(mouse_event.source_device_id());
  if (device_type_to_observe) {
    if (NotifyMouseEventObserving(mouse_event, *device_type_to_observe)) {
      return DiscardEvent(continuation);
    }

    // Otherwise, the flags must be cleared for the remappable buttons so they
    // do not affect the application while the mouse is meant to be observed.
    ui::MouseEvent rewritten_event = mouse_event;
    const int remappable_flags =
        GetRemappableMouseEventFlags(*device_type_to_observe);
    rewritten_event.set_flags(rewritten_event.flags() & ~remappable_flags);
    rewritten_event.set_changed_button_flags(
        rewritten_event.changed_button_flags() & ~remappable_flags);
    return SendEvent(continuation, &rewritten_event);
  }

  return SendEvent(continuation, &mouse_event);
}

ui::EventDispatchDetails PeripheralCustomizationEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  DCHECK(features::IsPeripheralCustomizationEnabled());

  if (event.IsMouseEvent()) {
    return RewriteMouseEvent(*event.AsMouseEvent(), continuation);
  }

  if (event.IsKeyEvent()) {
    return RewriteKeyEvent(*event.AsKeyEvent(), continuation);
  }

  return SendEvent(continuation, &event);
}

void PeripheralCustomizationEventRewriter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PeripheralCustomizationEventRewriter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
