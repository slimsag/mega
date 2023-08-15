// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/features.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/field_trial_params.h"
#import "ui/base/device_form_factor.h"

BASE_FEATURE(kEnablePinnedTabs,
             "EnablePinnedTabs",
             base::FEATURE_DISABLED_BY_DEFAULT);


bool IsPinnedTabsEnabled() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return false;
  }
  return base::FeatureList::IsEnabled(kEnablePinnedTabs);
}
