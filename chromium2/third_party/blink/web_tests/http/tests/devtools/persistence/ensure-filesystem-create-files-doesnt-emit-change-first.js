// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {
  TestRunner.addResult(`Verify that fs.createFile is creating UISourceCode atomically with content`);

  var folderLocation = '/var/test';
  await (new BindingsTestRunner.TestFileSystem(folderLocation)).reportCreatedPromise();

  Workspace.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, async event => {
    var uiSourceCode = event.data;
    var content = await uiSourceCode.requestContent();
    TestRunner.addResult('Added: ' + uiSourceCode.url());
    TestRunner.addResult('With content: ' + content.content);
    TestRunner.completeTest();
  });

  var fsWorkspaceBinding = await Workspace.workspace.project('file://' + folderLocation);
  fsWorkspaceBinding.createFile('', 'test.txt', 'file content');
})()
