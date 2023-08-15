// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/util.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

// static
extensions::BrowserContextKeyedAPIFactory<EventManager>*
EventManager::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<EventManager>>
      instance;
  return instance.get();
}

// static
EventManager* EventManager::Get(content::BrowserContext* browser_context) {
  return extensions::BrowserContextKeyedAPIFactory<EventManager>::Get(
      browser_context);
}

EventManager::EventManager(content::BrowserContext* context)
    : event_router_(context), browser_context_(context) {}

EventManager::~EventManager() = default;

EventManager::RegisterEventResult EventManager::RegisterExtensionForEvent(
    extensions::ExtensionId extension_id,
    crosapi::TelemetryEventCategoryEnum category) {
  // This is a noop in case there is already an existing observation.
  if (event_router_.IsExtensionObservingForCategory(extension_id, category)) {
    // Early return in case the category is already observed by the extension.
    return kSuccess;
  }

  if (app_ui_observers_.find(extension_id) == app_ui_observers_.end()) {
    auto observer = CreateAppUiObserver(extension_id);
    if (!observer) {
      return kAppUiClosed;
    }

    app_ui_observers_.emplace(extension_id, std::move(observer));
  }

  GetRemoteService()->AddEventObserver(
      category, event_router_.GetPendingRemoteForCategoryAndExtension(
                    category, extension_id));
  return kSuccess;
}

void EventManager::RemoveObservationsForExtensionAndCategory(
    extensions::ExtensionId extension_id,
    crosapi::TelemetryEventCategoryEnum category) {
  event_router_.ResetReceiversOfExtensionByCategory(extension_id, category);
  if (!event_router_.IsExtensionObserving(extension_id)) {
    app_ui_observers_.erase(extension_id);
  }
}

void EventManager::IsEventSupported(
    crosapi::TelemetryEventCategoryEnum category,
    crosapi::TelemetryEventService::IsEventSupportedCallback callback) {
  GetRemoteService()->IsEventSupported(category, std::move(callback));
}

mojo::Remote<crosapi::TelemetryEventService>& EventManager::GetRemoteService() {
  if (!remote_event_service_strategy_) {
    remote_event_service_strategy_ = RemoteEventServiceStrategy::Create();
  }
  return remote_event_service_strategy_->GetRemoteService();
}

void EventManager::OnAppUiClosed(extensions::ExtensionId extension_id) {
  // Try to find another open UI.
  auto observer = CreateAppUiObserver(extension_id);
  if (observer) {
    app_ui_observers_.insert_or_assign(extension_id, std::move(observer));
    return;
  }

  app_ui_observers_.erase(extension_id);
  event_router_.ResetReceiversForExtension(extension_id);
}

std::unique_ptr<AppUiObserver> EventManager::CreateAppUiObserver(
    extensions::ExtensionId extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    // If the extension has been unloaded from the registry, there
    // won't be any related app UI.
    return nullptr;
  }
  content::WebContents* contents =
      FindTelemetryExtensionOpenAndSecureAppUi(browser_context_, extension);
  if (!contents) {
    return nullptr;
  }

  return std::make_unique<AppUiObserver>(
      contents,
      extensions::ExternallyConnectableInfo::Get(extension)->matches.Clone(),
      // Unretained is safe here because `this` will own the observer.
      base::BindOnce(&EventManager::OnAppUiClosed, base::Unretained(this),
                     extension_id));
}

}  // namespace chromeos
