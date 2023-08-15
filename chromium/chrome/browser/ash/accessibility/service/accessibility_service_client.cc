// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/accessibility_service_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/service/accessibility_service_devtools_delegate.h"
#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/ash/accessibility/service/tts_client_impl.h"
#include "chrome/browser/ash/accessibility/service/user_interface_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ash {

AccessibilityServiceClient::AccessibilityServiceClient() = default;

AccessibilityServiceClient::~AccessibilityServiceClient() {
  Reset();
}

void AccessibilityServiceClient::BindAutomation(
    mojo::PendingAssociatedRemote<ax::mojom::Automation> automation,
    mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client) {
  automation_client_->Bind(std::move(automation), std::move(automation_client));
}

void AccessibilityServiceClient::BindTts(
    mojo::PendingReceiver<ax::mojom::Tts> tts_receiver) {
  tts_client_->Bind(std::move(tts_receiver));
}

void AccessibilityServiceClient::BindUserInterface(
    mojo::PendingReceiver<ax::mojom::UserInterface> ui_receiver) {
  user_interface_client_->Bind(std::move(ui_receiver));
}

void AccessibilityServiceClient::SetProfile(content::BrowserContext* profile) {
  // If the profile has changed we will need to disconnect from the previous
  // service, get the service keyed to this profile, and if any features were
  // enabled, re-establish the service connection with those features. Note that
  // this matches behavior in AccessibilityExtensionLoader::SetProfile, which
  // does the parallel logic with the extension system.
  if (profile_ == profile)
    return;

  Reset();
  profile_ = profile;
  if (profile_ && enabled_features_.size())
    LaunchAccessibilityServiceAndBind();
}

void AccessibilityServiceClient::SetChromeVoxEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kChromeVox,
                            enabled);
}

void AccessibilityServiceClient::SetSelectToSpeakEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kSelectToSpeak,
                            enabled);
}

void AccessibilityServiceClient::SetSwitchAccessEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kSwitchAccess,
                            enabled);
}

void AccessibilityServiceClient::SetAutoclickEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kAutoClick,
                            enabled);
}

void AccessibilityServiceClient::SetMagnifierEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kMagnifier,
                            enabled);
}

void AccessibilityServiceClient::SetDictationEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kDictation,
                            enabled);
}

void AccessibilityServiceClient::Reset() {
  at_controller_.reset();
  automation_client_.reset();
  tts_client_.reset();
  devtools_agent_hosts_.clear();
  user_interface_client_.reset();
}

void AccessibilityServiceClient::EnableAssistiveTechnology(
    ax::mojom::AssistiveTechnologyType type,
    bool enabled) {
  // Update the list of enabled features.
  auto iter =
      std::find(enabled_features_.begin(), enabled_features_.end(), type);
  // If a feature's state isn't being changed, do nothing.
  if ((enabled && iter != enabled_features_.end()) ||
      (!enabled && iter == enabled_features_.end())) {
    return;
  } else if (enabled && iter == enabled_features_.end()) {
    enabled_features_.push_back(type);
    AccessibilityManager::Get()->InitializeFocusRings(type);
  } else if (!enabled && iter != enabled_features_.end()) {
    enabled_features_.erase(iter);
    AccessibilityManager::Get()->RemoveFocusRings(type);
  }

  if (!enabled && !at_controller_.is_bound()) {
    // No need to launch the service, nothing is enabled.
    return;
  }

  if (at_controller_.is_bound()) {
    at_controller_->EnableAssistiveTechnology(enabled_features_);
    // Create or destroy devtools agent.
    if (enabled) {
      CreateDevToolsAgentHost(type);
    } else {
      auto it = devtools_agent_hosts_.find(type);
      if (it != devtools_agent_hosts_.end()) {
        // Detach all sessions before destroying.
        it->second->ForceDetachAllSessions();
        devtools_agent_hosts_.erase(it);
      }
    }
    return;
  }

  // A new feature is enabled but the service isn't running yet.
  LaunchAccessibilityServiceAndBind();
}

void AccessibilityServiceClient::LaunchAccessibilityServiceAndBind() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_)
    return;

  ax::AccessibilityServiceRouter* router =
      ax::AccessibilityServiceRouterFactory::GetForBrowserContext(
          static_cast<content::BrowserContext*>(profile_));

  if (!router) {
    return;
  }

  automation_client_ = std::make_unique<AutomationClientImpl>();
  tts_client_ = std::make_unique<TtsClientImpl>(profile_);
  user_interface_client_ = std::make_unique<UserInterfaceImpl>();

  // Bind the AXServiceClient before enabling features.
  router->BindAccessibilityServiceClient(
      service_client_.BindNewPipeAndPassRemote());
  router->BindAssistiveTechnologyController(
      at_controller_.BindNewPipeAndPassReceiver(), enabled_features_);
  // Create agent host for all enabled features.
  for (auto& type : enabled_features_) {
    CreateDevToolsAgentHost(type);
  }
}

void AccessibilityServiceClient::CreateDevToolsAgentHost(
    ax::mojom::AssistiveTechnologyType type) {
  auto host = content::DevToolsAgentHost::CreateForMojomDelegate(
      base::Uuid::GenerateRandomV4().AsLowercaseString(),
      // base::Unretained is safe because all agent hosts and
      // their delegates are deleted in the destructor of this class when
      // |hosts_| is cleared.
      std::make_unique<AccessibilityServiceDevToolsDelegate>(
          type,
          base::BindRepeating(&AccessibilityServiceClient::ConnectDevToolsAgent,
                              base::Unretained(this))));
  devtools_agent_hosts_.emplace(type, host);
}

void AccessibilityServiceClient::ConnectDevToolsAgent(
    ::mojo::PendingAssociatedReceiver<::blink::mojom::DevToolsAgent> agent,
    ax::mojom::AssistiveTechnologyType type) {
  if (!profile_) {
    return;
  }

  ax::AccessibilityServiceRouter* router =
      ax::AccessibilityServiceRouterFactory::GetForBrowserContext(
          static_cast<content::BrowserContext*>(profile_));
  if (router) {
    router->ConnectDevToolsAgent(std::move(agent), type);
  }
}

}  // namespace ash
