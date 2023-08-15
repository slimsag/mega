// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorAction, ActionChoice, GraphicsTablet, Keyboard, MetaKey, ModifierKey, Mouse, PointingStick, SimulateRightClickModifier, SixPackKeyInfo, SixPackShortcutModifier, Stylus, Touchpad, Vkey} from './input_device_settings_types.js';

const defaultSixPackKeyRemappings: SixPackKeyInfo = {
  pageDown: SixPackShortcutModifier.kSearch,
  pageUp: SixPackShortcutModifier.kSearch,
  del: SixPackShortcutModifier.kSearch,
  insert: SixPackShortcutModifier.kSearch,
  home: SixPackShortcutModifier.kAlt,
  end: SixPackShortcutModifier.kAlt,
};

export const fakeKeyboards: Keyboard[] = [
  {
    id: 0,
    deviceKey: 'test:key',
    name: 'ERGO K860',
    isExternal: true,
    metaKey: MetaKey.kCommand,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    settings: {
      modifierRemappings: {
        [ModifierKey.kControl]: ModifierKey.kCapsLock,
        [ModifierKey.kCapsLock]: ModifierKey.kAssistant,
      },
      topRowAreFkeys: false,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
    },
  },
  {
    id: 1,
    deviceKey: 'test:key',
    name: 'AT Translated Set 2 ',
    isExternal: false,
    metaKey: MetaKey.kSearch,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kAssistant,
      ModifierKey.kBackspace,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    settings: {
      modifierRemappings: {},
      topRowAreFkeys: true,
      suppressMetaFkeyRewrites: true,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
    },
  },
  {
    id: 8,
    deviceKey: 'test:key',
    name: 'Logitech G713 Aurora',
    isExternal: true,
    metaKey: MetaKey.kLauncher,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kAssistant,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    settings: {
      modifierRemappings: {[ModifierKey.kAlt]: ModifierKey.kAssistant},
      topRowAreFkeys: true,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
    },
  },
];

export const fakeKeyboards2: Keyboard[] = [
  {
    id: 9,
    deviceKey: 'test:key',
    name: 'Fake ERGO K860',
    isExternal: true,
    metaKey: MetaKey.kCommand,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    settings: {
      modifierRemappings: {
        [ModifierKey.kControl]: ModifierKey.kCapsLock,
        [ModifierKey.kCapsLock]: ModifierKey.kAssistant,
      },
      topRowAreFkeys: false,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
    },
  },
  {
    id: 10,
    deviceKey: 'test:key',
    name: 'Fake AT Translated Set 2 ',
    isExternal: false,
    metaKey: MetaKey.kSearch,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kAssistant,
      ModifierKey.kBackspace,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    settings: {
      modifierRemappings: {},
      topRowAreFkeys: true,
      suppressMetaFkeyRewrites: true,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
    },
  },
];

export const fakeTouchpads: Touchpad[] = [
  {
    id: 2,
    deviceKey: 'test:key',
    name: 'Default Touchpad',
    isExternal: false,
    isHaptic: true,
    settings: {
      sensitivity: 1,
      reverseScrolling: false,
      accelerationEnabled: false,
      tapToClickEnabled: false,
      threeFingerClickEnabled: false,
      tapDraggingEnabled: false,
      scrollSensitivity: 1,
      scrollAcceleration: false,
      hapticSensitivity: 1,
      hapticEnabled: false,
      simulateRightClick: SimulateRightClickModifier.kNone,
    },
  },
  {
    id: 3,
    deviceKey: 'test:key',
    name: 'Logitech T650',
    isExternal: true,
    isHaptic: false,
    settings: {
      sensitivity: 5,
      reverseScrolling: true,
      accelerationEnabled: true,
      tapToClickEnabled: true,
      threeFingerClickEnabled: true,
      tapDraggingEnabled: true,
      scrollSensitivity: 5,
      scrollAcceleration: true,
      hapticSensitivity: 5,
      hapticEnabled: true,
      simulateRightClick: SimulateRightClickModifier.kNone,
    },
  },
];

export const fakeTouchpads2: Touchpad[] = [
  {
    id: 11,
    deviceKey: 'test:key',
    name: 'Fake Default Touchpad',
    isExternal: false,
    isHaptic: true,
    settings: {
      sensitivity: 1,
      reverseScrolling: false,
      accelerationEnabled: false,
      tapToClickEnabled: false,
      threeFingerClickEnabled: false,
      tapDraggingEnabled: false,
      scrollSensitivity: 1,
      scrollAcceleration: false,
      hapticSensitivity: 1,
      hapticEnabled: false,
      simulateRightClick: SimulateRightClickModifier.kNone,
    },
  },
];

export const fakeMice: Mouse[] = [
  {
    id: 4,
    deviceKey: 'test:key',
    name: 'Razer Basilisk V3',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      reverseScrolling: true,
      accelerationEnabled: true,
      scrollSensitivity: 5,
      scrollAcceleration: true,
    },
  },
  {
    id: 5,
    deviceKey: 'test:key',
    name: 'MX Anywhere 2S',
    isExternal: false,
    settings: {
      swapRight: false,
      sensitivity: 1,
      reverseScrolling: false,
      accelerationEnabled: false,
      scrollSensitivity: 1,
      scrollAcceleration: false,
    },
  },
];

export const fakeMice2: Mouse[] = [
  {
    id: 13,
    deviceKey: 'test:key',
    name: 'Fake Razer Basilisk V3',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      reverseScrolling: true,
      accelerationEnabled: true,
      scrollSensitivity: 5,
      scrollAcceleration: true,
    },
  },
];

export const fakePointingSticks: PointingStick[] = [
  {
    id: 6,
    deviceKey: 'test:key',
    name: 'Default Pointing Stick',
    isExternal: false,
    settings: {
      swapRight: false,
      sensitivity: 1,
      accelerationEnabled: false,
    },
  },
  {
    id: 7,
    deviceKey: 'test:key',
    name: 'Lexmark-Unicomp FSR',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      accelerationEnabled: true,
    },
  },
];

export const fakePointingSticks2: PointingStick[] = [
  {
    id: 12,
    deviceKey: 'test:key',
    name: 'Fake Lexmark-Unicomp FSR',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      accelerationEnabled: true,
    },
  },
];

export const fakeStyluses: Stylus[] = [
  {
    id: 13,
    deviceKey: 'test:key',
    name: 'Apple Pencil 2nd generation',
  },
  {
    id: 14,
    deviceKey: 'test:key',
    name: 'Zebra ET8X',
  },
];

export const fakeGraphicsTablets: GraphicsTablet[] = [
  {
    id: 15,
    deviceKey: 'test:key',
    name: 'Wacom Cintiq 16',
    settings: {
      tabletButtonRemappings: [
        {
          name: 'Back Button',
          button: {
            vkey: Vkey.kNum0,
          },
          remappingAction: {
            action: AcceleratorAction.kCycleBackwardMru,
          },
        },
        {
          name: 'Forward Button',
          button: {
            vkey: Vkey.kNum1,
          },
          remappingAction: {
            action: AcceleratorAction.kCycleForwardMru,
          },
        },
      ],
      penButtonRemappings: [
        {
          name: 'Undo',
          button: {
            vkey: Vkey.kNum2,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
            },
          },
        },
        {
          name: 'Redo',
          button: {
            vkey: Vkey.kNum3,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 6,
            },
          },
        },
      ],
    },
  },
  {
    id: 16,
    deviceKey: 'test:key',
    name: 'XPPen Artist13.3 Pro',
    settings: {
      tabletButtonRemappings: [
        {
          name: 'Brightness Up',
          button: {
            vkey: Vkey.kNum0,
          },
          remappingAction: {
            action: AcceleratorAction.kBrightnessUp,
          },
        },
        {
          name: 'Brightness down',
          button: {
            vkey: Vkey.kNum1,
          },
          remappingAction: {
            action: AcceleratorAction.kBrightnessDown,
          },
        },
      ],
      penButtonRemappings: [
        {
          name: 'Copy',
          button: {
            vkey: Vkey.kNum2,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyC,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
            },
          },
        },
        {
          name: 'Paste',
          button: {
            vkey: Vkey.kNum3,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyV,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
            },
          },
        },
      ],
    },
  },
];

export const fakeMouseButtonActions: ActionChoice[] = [
  {actionId: AcceleratorAction.kCycleBackwardMru, name: 'Back'},
  {actionId: AcceleratorAction.kCycleForwardMru, name: 'Forward'},
  {actionId: AcceleratorAction.kLockScreen, name: 'Lock screen'},
  {actionId: AcceleratorAction.kToggleClipboardHistory, name: 'Open clipboard'},
  {actionId: AcceleratorAction.kToggleFullscreen, name: 'Fullscreen'},
  {actionId: AcceleratorAction.kVolumeMute, name: 'Mute'},
  {actionId: AcceleratorAction.kWindowMinimize, name: 'Minimize window'},
];

export const fakeGraphicsTabletButtonActions: ActionChoice[] = [
  {actionId: AcceleratorAction.kBrightnessDown, name: 'Brightness down'},
  {actionId: AcceleratorAction.kBrightnessUp, name: 'Brightness up'},
  {actionId: AcceleratorAction.kCycleBackwardMru, name: 'Back'},
  {actionId: AcceleratorAction.kCycleForwardMru, name: 'Forward'},
  {actionId: AcceleratorAction.kMagnifierZoomIn, name: 'Zoom in'},
  {actionId: AcceleratorAction.kMagnifierZoomOut, name: 'Zoom out'},
];
