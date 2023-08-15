// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_consumer.h"

// Presents the list of Google Family members of a user.
@interface FamilyPickerViewController
    : ChromeTableViewController <FamilyPickerConsumer>
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_H_
