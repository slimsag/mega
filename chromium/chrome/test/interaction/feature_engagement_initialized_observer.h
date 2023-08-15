// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_FEATURE_ENGAGEMENT_INITIALIZED_OBSERVER_H_
#define CHROME_TEST_INTERACTION_FEATURE_ENGAGEMENT_INITIALIZED_OBSERVER_H_

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/state_observer.h"

namespace feature_engagement {
class Tracker;
}

class Browser;

// Observes the initialization state of the `feature_engagement::Tracker`
// associated with a browser.
class FeatureEngagementInitializedObserver
    : public ui::test::StateObserver<bool> {
 public:
  explicit FeatureEngagementInitializedObserver(Browser* browser);
  ~FeatureEngagementInitializedObserver() override;

  // ui::test::StateObserver<bool>:
  bool GetStateObserverInitialState() const override;

 private:
  void OnTrackerInitialized(bool success);

  raw_ptr<feature_engagement::Tracker> tracker_ = nullptr;
  base::WeakPtrFactory<FeatureEngagementInitializedObserver> weak_ptr_factory_{
      this};
};

// Since there should only need to be one observer per context, a single
// identifier can be declared here.
DECLARE_STATE_IDENTIFIER_VALUE(FeatureEngagementInitializedObserver,
                               kFeatureEngagementInitializedState);

#endif  // CHROME_TEST_INTERACTION_FEATURE_ENGAGEMENT_INITIALIZED_OBSERVER_H_
