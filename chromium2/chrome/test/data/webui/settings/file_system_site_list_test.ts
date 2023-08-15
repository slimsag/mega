// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FileSystemSiteListElement, FileSystemGrant, OriginFileSystemGrants, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('FileSystemSettings_EnablePersistentPermissions', function() {
  let testElement: FileSystemSiteListElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  const origin1: string = 'https://a.com/';
  const origin2: string = 'https://b.com/';
  const filePath1: string = 'a/b';
  const directoryFilePath1: string = 'g/h/';

  const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: FileSystemGrant = {
    origin: origin1,
    filePath: directoryFilePath1,
    displayName: directoryFilePath1,
    isDirectory: true,
  };
  const TEST_FILE_SYSTEM_FILE_READ_GRANT: FileSystemGrant = {
    origin: origin1,
    filePath: filePath1,
    displayName: filePath1,
    isDirectory: false,
  };
  const TEST_FILE_SYSTEM_FILE_WRITE_GRANT: FileSystemGrant = {
    origin: origin2,
    filePath: filePath1,
    displayName: filePath1,
    isDirectory: false,
  };

  const FILE_SYSTEM_GRANTS: OriginFileSystemGrants[] = [
    {
      origin: origin1,
      viewGrants: [
        TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT,
        TEST_FILE_SYSTEM_FILE_READ_GRANT,
      ],
      editGrants: [],
    },
    {
      origin: origin2,
      viewGrants: [],
      editGrants: [TEST_FILE_SYSTEM_FILE_WRITE_GRANT],
    },
  ];

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      showPersistentPermissions: true,
    });
  });

  // Initialize the file-system-site-list element.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('file-system-site-list');
    document.body.appendChild(testElement);
  });

  test(
      'Filesystem grants are properly listed and then removed',
      async function() {
        browserProxy.setFileSystemGrants(FILE_SYSTEM_GRANTS);

        Router.getInstance().navigateTo(routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
        await browserProxy.whenCalled('getFileSystemGrants');
        flush();

        // Ensure that the list container element is populated.
        const fileSystemSiteEntries =
            testElement.shadowRoot!.querySelectorAll('file-system-site-entry');
        assertEquals(2, fileSystemSiteEntries.length);

        // Remove grants.
        const origin1SiteEntryItems =
            fileSystemSiteEntries[0]!.shadowRoot!.querySelectorAll(
                'file-system-site-entry-item');
        const removeGrantButton =
            origin1SiteEntryItems[0]!.shadowRoot!.querySelector<HTMLElement>(
                '#removeGrant');
        assertTrue(!!removeGrantButton);

        removeGrantButton.click();
        const [testOrigin, filePath] =
            await browserProxy.whenCalled('revokeFileSystemGrant');
        assertEquals(origin1, testOrigin);
        assertEquals(directoryFilePath1, filePath);
        assertEquals(1, browserProxy.getCallCount('revokeFileSystemGrant'));

        // Option menu icons populate for all origins with grants.
        const optionsMenuButtonOrigin1 =
            fileSystemSiteEntries[0]!.shadowRoot!.querySelector<HTMLElement>(
                '.icon-more-vert');
        assertTrue(!!optionsMenuButtonOrigin1);

        const optionsMenuButtonOrigin2 =
            fileSystemSiteEntries[1]!.shadowRoot!.querySelector<HTMLElement>(
                '.icon-more-vert');
        assertTrue(!!optionsMenuButtonOrigin2);

        // Navigate to the site details page for a given origin.
        optionsMenuButtonOrigin2.click();
        const menu = testElement.$.menu.get();
        const viewSiteDetailsButton =
            menu!.querySelector<HTMLElement>('#viewSiteDetails');
        assertTrue(!!viewSiteDetailsButton);
        viewSiteDetailsButton.click();
        assertEquals(
            routes.SITE_SETTINGS_SITE_DETAILS.path,
            Router.getInstance().getCurrentRoute().path);
        assertEquals(
            origin2, Router.getInstance().getQueryParameters().get('site'));

        // Remove all of an origin's granted permissions.
        optionsMenuButtonOrigin1.click();
        const updatedMenu = testElement.$.menu.get();
        const removeGrantsButton =
            updatedMenu.querySelector<HTMLElement>('#removeGrants');
        assertTrue(!!removeGrantsButton);
        removeGrantsButton.click();
        const testOrigin1 =
            await browserProxy.whenCalled('revokeFileSystemGrants');
        assertEquals(origin1, testOrigin1);
        assertEquals(1, browserProxy.getCallCount('revokeFileSystemGrants'));

        // Assert that when webUIListenerCallback is triggered, that the UI
        // is updated.
        webUIListenerCallback(
            'contentSettingChooserPermissionChanged', 'file-system-write');
        assertEquals(
            2,
            testElement.shadowRoot!.querySelectorAll('file-system-site-entry')
                .length);
      });
});
