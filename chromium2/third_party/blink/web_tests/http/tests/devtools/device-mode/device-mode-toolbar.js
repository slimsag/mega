// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {DeviceModeTestRunner} from 'device_mode_test_runner';

(async function() {
  TestRunner.addResult(`Test toolbar state when switching modes.\n`);

  var phoneA = DeviceModeTestRunner.buildFakePhone();
  var view = new Emulation.DeviceModeView();
  var toolbar = view.toolbar;
  var model = view.model;
  var viewportSize = new UI.Size(800, 600);
  model.setAvailableSize(viewportSize, viewportSize);

  // Check that default model has type None.
  dumpInfo();

  model.emulate(Emulation.DeviceModeModel.Type.None, null, null);
  dumpType();
  toolbar.switchToResponsive();
  dumpInfo();

  model.emulate(Emulation.DeviceModeModel.Type.None, null, null);
  dumpType();
  toolbar.emulateDevice(phoneA);
  dumpInfo();

  toolbar.switchToResponsive();
  dumpInfo();

  toolbar.emulateDevice(phoneA);
  dumpInfo();

  function dumpType() {
    TestRunner.addResult(`Type: ${model.type()}, Device: ${model.device() ? model.device().title : '<no device>'}`);
  }

  function dumpInfo() {
    dumpType();
    TestRunner.addResult(`Rotate: ${toolbar.modeButton.enabled ? 'enabled': 'disabled'}, Width/Height: ${!toolbar.widthInput.disabled ? 'enabled': 'disabled'}`);
  }

  TestRunner.completeTest();
})();
