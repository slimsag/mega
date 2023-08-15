// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;

namespace views {
class WebView;
}

// Implements the Search Engine Choice dialog as a View. The view contains a
// WebView into which is loaded a WebUI page which renders the actual dialog
// content.
class SearchEngineChoiceDialogView : public views::View {
 public:
  METADATA_HEADER(SearchEngineChoiceDialogView);
  explicit SearchEngineChoiceDialogView(Browser* browser);
  ~SearchEngineChoiceDialogView() override;

  // Initialize SearchEngineChoiceDialogView's web_view_ element.
  void Initialize();

 private:
  // Show the dialog widget. `content_height` sets setting the dialog's height.
  void ShowNativeView(int content_height);

  // Close the dialog widget.
  void CloseView();

  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<Browser> browser_;
  base::WeakPtrFactory<SearchEngineChoiceDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_DIALOG_VIEW_H_
