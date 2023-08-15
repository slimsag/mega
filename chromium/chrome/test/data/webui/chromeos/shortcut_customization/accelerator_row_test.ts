// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_row.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorRowElement} from 'chrome://shortcut-customization/js/accelerator_row.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {InputKeyElement} from 'chrome://shortcut-customization/js/input_key.js';
import {stringToMojoString16} from 'chrome://shortcut-customization/js/mojo_utils.js';
import {AcceleratorSource, AcceleratorState, LayoutStyle, Modifier, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createTextAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

export function initAcceleratorRowElement(): AcceleratorRowElement {
  const element = document.createElement('accelerator-row');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('acceleratorRowTest', function() {
  let rowElement: AcceleratorRowElement|null = null;
  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    // Set up manager.
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    if (rowElement) {
      rowElement.remove();
    }
    rowElement = null;
  });

  test('LoadsBasicRow', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    rowElement = initAcceleratorRowElement();
    const acceleratorInfo1 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2 = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';

    rowElement.acceleratorInfos = accelerators;
    rowElement.description = description;
    rowElement.layoutStyle = LayoutStyle.kDefault;
    await flush();
    const acceleratorElements =
        rowElement!.shadowRoot!.querySelectorAll('accelerator-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description,
        rowElement!.shadowRoot!.querySelector(
                                   '#descriptionText')!.textContent!.trim());

    const keys1: NodeListOf<InputKeyElement> =
        acceleratorElements[0]!.shadowRoot!.querySelectorAll('input-key');
    // SHIFT + CONTROL + g
    assertEquals(3, keys1.length);
    assertEquals(
        'ctrl',
        keys1[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'shift',
        keys1[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'g', keys1[2]!.shadowRoot!.querySelector('#key')!.textContent!.trim());

    const keys2 =
        acceleratorElements[1]!.shadowRoot!.querySelectorAll('input-key');
    // CONTROL + c
    assertEquals(2, keys2.length);
    assertEquals(
        'ctrl',
        keys2[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'c', keys2[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
  });

  test('ShowDialogOnClickWhenCustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    rowElement = initAcceleratorRowElement();
    waitAfterNextRender(rowElement);

    const acceleratorInfo1 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const accelerators = [acceleratorInfo1];
    const description = 'test shortcut';

    rowElement.acceleratorInfos = accelerators;
    rowElement.description = description;
    rowElement.source = AcceleratorSource.kAsh;
    rowElement.action = 0;
    rowElement.layoutStyle = LayoutStyle.kDefault;

    let showDialogListenerCalled = false;
    rowElement.addEventListener('show-edit-dialog', () => {
      showDialogListenerCalled = true;
    });

    await flushTasks();

    const acceleratorViewElement =
        rowElement!.shadowRoot!.querySelectorAll('accelerator-view');
    assertEquals(1, acceleratorViewElement.length);
    const editButton = strictQuery(
        '.edit-button', acceleratorViewElement[0]!.shadowRoot,
        CrIconButtonElement);

    editButton.click();

    await flushTasks();

    assertTrue(showDialogListenerCalled);
  });

  test('EditIconHiddenWhenCustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    rowElement = initAcceleratorRowElement();
    waitAfterNextRender(rowElement);

    const acceleratorInfo1 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const accelerators = [acceleratorInfo1];
    const description = 'test shortcut';

    rowElement.acceleratorInfos = accelerators;
    rowElement.description = description;
    rowElement.source = AcceleratorSource.kAsh;
    rowElement.action = 0;
    rowElement.layoutStyle = LayoutStyle.kDefault;

    await flushTasks();

    const acceleratorViewElement =
        rowElement!.shadowRoot!.querySelectorAll('accelerator-view');
    assertEquals(1, acceleratorViewElement.length);

    const editIconContainerElement = strictQuery(
        '.edit-icon-container', acceleratorViewElement[0]!.shadowRoot,
        HTMLDivElement);


    assertFalse(isVisible(editIconContainerElement));
  });

  test('ShowTextAccelerator', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    rowElement = initAcceleratorRowElement();

    const accelerators = [createTextAcceleratorInfo([{
      text: stringToMojoString16('ctrl'),
      type: TextAcceleratorPartType.kModifier,
    }])];

    rowElement.acceleratorInfos = accelerators;
    rowElement.layoutStyle = LayoutStyle.kText;
    await flush();

    const acceleratorElements =
        rowElement!.shadowRoot!.querySelectorAll('accelerator-view');
    // Because rowElement.layoutStyle is kText, we don't expect there to be any
    // 'accelerator-view' elements shown. Instead, 'text-accelerator' elements
    // will be shown.
    assertEquals(0, acceleratorElements.length);

    const textAccelElement =
        rowElement!.shadowRoot!.querySelector('text-accelerator');
    assertTrue(!!textAccelElement);
    const textWrapper = textAccelElement!.shadowRoot!.querySelector(
                            '#text-wrapper') as HTMLDivElement;
    assertTrue(!!textWrapper);
  });

  test('LoadBasicRowEvenWhenAccelTextIsPresent', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    rowElement = initAcceleratorRowElement();
    const acceleratorInfo1 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2 = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';

    rowElement.acceleratorInfos = accelerators;
    rowElement.description = description;
    rowElement.layoutStyle = LayoutStyle.kDefault;
    await flush();

    const acceleratorElements =
        rowElement!.shadowRoot!.querySelectorAll('accelerator-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description,
        rowElement!.shadowRoot!.querySelector(
                                   '#descriptionText')!.textContent!.trim());

    // Because rowElement.layoutStyle is kDefault, we don't expect any
    // 'text-accelerator' elements to be shown, even though the property
    // rowElement.acceleratorText is set.
    const textAccelElement =
        rowElement!.shadowRoot!.querySelector('text-accelerator');
    assertFalse(!!textAccelElement);

    const keys1: NodeListOf<InputKeyElement> =
        acceleratorElements[0]!.shadowRoot!.querySelectorAll('input-key');
    // SHIFT + CONTROL + g
    assertEquals(3, keys1.length);

    const keys2 =
        acceleratorElements[1]!.shadowRoot!.querySelectorAll('input-key');
    // CONTROL + c
    assertEquals(2, keys2.length);
  });

  test('ElementFocusableWhenCustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    rowElement = initAcceleratorRowElement();

    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');
    acceleratorInfo.state = AcceleratorState.kEnabled;

    rowElement.acceleratorInfos = [acceleratorInfo];
    rowElement.description = 'test shortcut';
    rowElement.layoutStyle = LayoutStyle.kDefault;
    await flush();

    const containerElement =
        strictQuery('#container', rowElement.shadowRoot, HTMLTableRowElement);
    assertEquals(0, containerElement.tabIndex);
  });

  test('ElementFocusableWhenCustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    rowElement = initAcceleratorRowElement();

    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');
    acceleratorInfo.state = AcceleratorState.kEnabled;

    rowElement.acceleratorInfos = [acceleratorInfo];
    rowElement.description = 'test shortcut';
    rowElement.layoutStyle = LayoutStyle.kDefault;
    await flush();

    const containerElement =
        strictQuery('#container', rowElement.shadowRoot, HTMLTableRowElement);
    assertEquals(-1, containerElement.tabIndex);
  });
});
