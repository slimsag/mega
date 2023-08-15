// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

(async function() {
  TestRunner.addResult('Tests accessibility in Network conditions view using the axe-core linter.');

  const view = 'network.config';
  await UI.viewManager.showView(view);
  const widget = await UI.viewManager.view(view).widget();
  await AxeCoreTestRunner.runValidation(widget.element);

  TestRunner.completeTest();
})();
