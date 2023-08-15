// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.ui.base.WindowAndroid;

/** Allows for launching {@link DeviceLockActivity} in modularized code. */
public interface DeviceLockActivityLauncher {
    /**
     * Launches the {@link DeviceLockActivity} to set a device lock for data privacy.
     * @param context The context to launch the {@link DeviceLockActivity} with.
     * @param selectedAccount The account that will be used for the reauthentication challenge, or
     *                null if reauthentication is not needed.
     * @param windowAndroid The host activity's {@link WindowAndroid}.
     * @param callback A callback to run after the {@link DeviceLockActivity} finishes.
     */
    void launchDeviceLockActivity(Context context, @Nullable String selectedAccount,
            WindowAndroid windowAndroid, WindowAndroid.IntentCallback callback);
}
