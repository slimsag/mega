// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

SearchEngineChoiceTabHelper::~SearchEngineChoiceTabHelper() = default;

SearchEngineChoiceTabHelper::SearchEngineChoiceTabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchEngineChoiceTabHelper>(*web_contents) {
  CHECK(base::FeatureList::IsEnabled(switches::kSearchEngineChoice));
}

void SearchEngineChoiceTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return;
  }

  // Only valid top frame and committed navigations are considered.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Don't show the dialog on top of any sub page of the settings page.
  if (navigation_handle->GetURL().host() == chrome::kChromeUISettingsHost) {
    return;
  }

  Browser* browser =
      chrome::FindBrowserWithWebContents(navigation_handle->GetWebContents());
  if (!SearchEngineChoiceService::ShouldDisplayDialog(CHECK_DEREF(browser))) {
    return;
  }
  ShowSearchEngineChoiceDialog(*browser);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchEngineChoiceTabHelper);
