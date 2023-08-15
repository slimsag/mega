// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_H_

#include <ostream>

#include "chrome/browser/apps/app_service/package_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace apps {

// Indicates the status of the app installation that the promise app represents.
enum class PromiseStatus {
  kUnknown,
  kPending,     // Waiting for the installation process to start.
  kInstalling,  // Installing app package.
  kRemove,      // Marking the promise app for deletion.
};

std::string EnumToString(PromiseStatus);

// A promise app is a barebones app object created to show an app's icon and
// name in the Launcher/Shelf while the package is currently installing
// or pending installation. Each pending package installation is represented by
// its own promise app.
struct PromiseApp {
 public:
  explicit PromiseApp(const apps::PackageId& package_id);
  ~PromiseApp();

  bool operator==(const PromiseApp&) const;

  PackageId package_id;

  absl::optional<std::string> name;
  absl::optional<float> progress;
  PromiseStatus status = PromiseStatus::kUnknown;

  // Hide the promise app from the Launcher/ Shelf by default. Only show
  // it when we have enough information about the installing package (e.g. name,
  // icon).
  absl::optional<bool> should_show;

  std::unique_ptr<PromiseApp> Clone() const;
};

std::ostream& operator<<(std::ostream& out, const PromiseApp& promise_app);

using PromiseAppPtr = std::unique_ptr<PromiseApp>;

class PromiseAppIcon {
 public:
  PromiseAppIcon();
  ~PromiseAppIcon();
  PromiseAppIcon(const PromiseAppIcon&) = delete;
  PromiseAppIcon& operator=(const PromiseAppIcon&) = delete;

  // Store the icon as a SkBitmap, which will form one of the several
  // representations of an ImageSkia for a DIP size.
  SkBitmap icon;
  int width_in_pixels;
};

using PromiseAppIconPtr = std::unique_ptr<PromiseAppIcon>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_H_
