// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-saved-printers' is a list container for Saved
 * Printers.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared.css.js';
import './cups_printer_types.js';
import './cups_printers_browser_proxy.js';
import './cups_printers_entry.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {matchesSearchTerm, sortPrinters} from './cups_printer_dialog_util.js';
import {PrinterListEntry} from './cups_printer_types.js';
import {PrinterSettingsUserAction, recordPrinterSettingsUserAction} from './cups_printers.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryListMixin} from './cups_printers_entry_list_mixin.js';
import {getTemplate} from './cups_saved_printers.html.js';
import {getStatusReasonFromPrinterStatus, PrinterStatus, PrinterStatusReason} from './printer_status.js';

/**
 * If the Show more button is visible, the minimum number of printers we show
 * is 3.
 */
const MIN_VISIBLE_PRINTERS: number = 3;

/**
 * The amount of time Printer settings is open until it switches to a longer
 * delay between each printer status query.
 */
const PRINTER_STATUS_QUERY_SHORT_DELAY_DURATION_MS: number = 120000;

/**
 * The inclusive range for the delay between printer status queries. This
 * shorter interval is used when Printer settings initially opens. This will
 * capture changes when the user will most likely be interacting with the
 * printer.
 */
export const PRINTER_STATUS_QUERY_SHORT_DELAY_RANGE_MS: number[] =
    [15000, 25000];

/**
 * The inclusive range of the delay between printer status queries.
 */
const PRINTER_STATUS_QUERY_LONG_DELAY_RANGE_MS: number[] = [60000, 80000];

/**
 * Move a printer's position in |printerArr| from |fromIndex| to |toIndex|.
 */
function moveEntryInPrinters(
    printerArr: PrinterListEntry[], fromIndex: number, toIndex: number) {
  const element = printerArr[fromIndex];
  printerArr.splice(fromIndex, 1);
  printerArr.splice(toIndex, 0, element);
}

const SettingsCupsSavedPrintersElementBase =
    CupsPrintersEntryListMixin(WebUiListenerMixin(PolymerElement));

export class SettingsCupsSavedPrintersElement extends
    SettingsCupsSavedPrintersElementBase {
  static get is(): string {
    return 'settings-cups-saved-printers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Search term for filtering |savedPrinters|.
       */
      searchTerm: {
        type: String,
        value: '',
      },

      activePrinter: {
        type: Object,
        notify: true,
      },

      printersCount: {
        type: Number,
        computed: 'getFilteredPrintersLength_(filteredPrinters_.*)',
        notify: true,
      },

      activePrinterListEntryIndex_: {
        type: Number,
        value: -1,
      },

      /**
       * List of printers filtered through a search term.
       */
      filteredPrinters_: {
        type: Array,
        value: () => [],
      },

      /**
       * Array of new PrinterListEntry's that were added during this session.
       */
      newPrinters_: {
        type: Array,
        value: () => [],
      },

      /**
       * Keeps track of whether the user has tapped the Show more button. A
       * search term will expand the collapsed list, so we need to keep track of
       * whether the list expanded because of a search term or because the user
       * tapped on the Show more button.
       */
      hasShowMoreBeenTapped_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       */
      listBlurred_: Boolean,

      /**
       * The cache of printer status reasons. Used by printer status entries to
       * look up their current printer status.
       */
      printerStatusReasonCache_: {
        type: Map<string, PrinterStatusReason>,
        value() {
          return new Map();
        },
      },

      /**
       * Determines whether to use the short or long delay between printer
       * status queries.
       */
      useShortDelayInterval_: {
        type: Boolean,
        value: true,
      },

      /**
       * True when the "printer-settings-printer-status" feature flag is
       * enabled.
       */
      isPrinterSettingsPrinterStatusEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean(
              'isPrinterSettingsPrinterStatusEnabled');
        },
        readOnly: true,
      },

      /**
       * True when the "printer-settings-revamp" feature flag is enabled.
       */
      isPrinterSettingsRevampEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isPrinterSettingsRevampEnabled');
        },
        readOnly: true,
        reflectToAttribute: true,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onSearchOrPrintersChanged_(savedPrinters.*, searchTerm,' +
          'hasShowMoreBeenTapped_, newPrinters_.*)',
      'fetchPrinterStatuses_(savedPrinters.splices)',
    ];
  }

  activePrinter: CupsPrinterInfo|null;
  printersCount: number;
  searchTerm: string;

  private activePrinterListEntryIndex_: number;
  private browserProxy_: CupsPrintersBrowserProxy;
  private filteredPrinters_: PrinterListEntry[];
  private hasShowMoreBeenTapped_: boolean;
  private lastFocused_: Object;
  private listBlurred_: boolean;
  private newPrinters_: PrinterListEntry[];
  private visiblePrinterCounter_: number;
  private printerStatusReasonCache_: Map<string, PrinterStatusReason>;
  private useShortDelayInterval_: boolean;
  private isPrinterSettingsPrinterStatusEnabled_: boolean;

  constructor() {
    super();

    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();

    // The number of printers we display if hidden printers are allowed.
    // MIN_VISIBLE_PRINTERS is the default value and we never show fewer
    // printers if the Show more button is visible.
    this.visiblePrinterCounter_ = MIN_VISIBLE_PRINTERS;
  }

  override ready(): void {
    super.ready();

    this.addEventListener(
        'open-action-menu',
        (event: CustomEvent<{target: HTMLElement, item: PrinterListEntry}>) => {
          this.onOpenActionMenu_(event);
        });

    if (this.isPrinterSettingsPrinterStatusEnabled_) {
      this.startPrinterStatusQueryTimer_();

      // After Printer settings is open for a set amount of time, switch to the
      // longer printer status delay.
      setTimeout(
          () => this.useShortDelayInterval_ = false,
          PRINTER_STATUS_QUERY_SHORT_DELAY_DURATION_MS);
    }
  }

  /**
   * Redoes the search whenever |searchTerm| or |savedPrinters| changes.
   */
  private onSearchOrPrintersChanged_(): void {
    if (!this.savedPrinters) {
      return;
    }

    const updatedPrinters = this.getVisiblePrinters_();

    this.updateList(
        'filteredPrinters_',
        (printer: PrinterListEntry) => printer.printerInfo.printerId,
        updatedPrinters);
  }

  private onOpenActionMenu_(
      e: CustomEvent<{target: HTMLElement, item: PrinterListEntry}>) {
    const item = e.detail.item;
    this.activePrinterListEntryIndex_ = this.savedPrinters.findIndex(
        (printer: PrinterListEntry) =>
            printer.printerInfo.printerId === item.printerInfo.printerId);
    this.activePrinter =
        this.get(['savedPrinters', this.activePrinterListEntryIndex_])
            .printerInfo;

    const target = e.detail.target;
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(target);
  }

  private onEditClick_(): void {
    // Event is caught by 'settings-cups-printers'.
    const editCupsPrinterDetailsEvent =
        new CustomEvent('edit-cups-printer-details', {
          bubbles: true,
          composed: true,
        });
    this.dispatchEvent(editCupsPrinterDetailsEvent);
    this.closeActionMenu_();
    recordPrinterSettingsUserAction(PrinterSettingsUserAction.EDIT_PRINTER);
  }

  private onRemoveClick_(): void {
    // Remove this printer's current status reason from the cache so a stale
    // status isn't shown if the printer is added back.
    this.printerStatusReasonCache_.delete(this.activePrinter!.printerId);
    this.browserProxy_.removeCupsPrinter(
        this.activePrinter!.printerId, this.activePrinter!.printerName);
    recordSettingChange();
    this.activePrinter = null;
    this.activePrinterListEntryIndex_ = -1;
    this.closeActionMenu_();
    recordPrinterSettingsUserAction(PrinterSettingsUserAction.REMOVE_PRINTER);
  }

  private onShowMoreClick_(): void {
    this.hasShowMoreBeenTapped_ = true;
  }

  /**
   * Gets the printers to be shown in the UI. These printers are filtered
   * by the search term, alphabetically sorted (if applicable), and are the
   * printers not hidden by the Show more section.
   */
  private getVisiblePrinters_(): PrinterListEntry[] {
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |savedPrinters|.
    const updatedPrinters = this.searchTerm ?
        this.savedPrinters.filter(
            (item: PrinterListEntry) =>
                matchesSearchTerm(item.printerInfo, this.searchTerm)) :
        this.savedPrinters.slice();

    updatedPrinters.sort(sortPrinters);

    this.moveNewlyAddedPrinters_(updatedPrinters, 0 /* toIndex */);

    if (this.shouldPrinterListBeCollapsed_()) {
      // If the Show more button is visible, we only display the first
      // N < |visiblePrinterCounter_| printers and the rest are hidden.
      return updatedPrinters.filter(
          (_: PrinterListEntry, idx: number) =>
              idx < this.visiblePrinterCounter_);
    }
    return updatedPrinters;
  }

  private closeActionMenu_(): void {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  /**
   * @return Returns true if the no search message should be visible.
   */
  private showNoSearchResultsMessage_(): boolean {
    return !!this.searchTerm && !this.filteredPrinters_.length;
  }

  override onSavedPrintersAdded(addedPrinters: PrinterListEntry[]) {
    const currArr = this.newPrinters_.slice();
    for (const printer of addedPrinters) {
      this.visiblePrinterCounter_++;
      currArr.push(printer);
    }

    this.set('newPrinters_', currArr);
  }

  override onSavedPrintersRemoved(removedPrinters: PrinterListEntry[]) {
    const currArr = this.newPrinters_.slice();
    for (const printer of removedPrinters) {
      const newPrinterRemovedIdx = currArr.findIndex(
          p => p.printerInfo.printerId === printer.printerInfo.printerId);
      // If the removed printer is a recently added printer, remove it from
      // |currArr|.
      if (newPrinterRemovedIdx > -1) {
        currArr.splice(newPrinterRemovedIdx, 1);
      }

      this.visiblePrinterCounter_ =
          Math.max(MIN_VISIBLE_PRINTERS, --this.visiblePrinterCounter_);
    }

    this.set('newPrinters_', currArr);
  }

  /**
   * Keeps track of whether the Show more button should be visible which means
   * that the printer list is collapsed. There are two ways a collapsed list
   * may be expanded: the Show more button is tapped or if there is a search
   * term.
   * @return True if the printer list should be collapsed.
   */
  private shouldPrinterListBeCollapsed_(): boolean {
    // If |searchTerm| is set, never collapse the list.
    if (this.searchTerm) {
      return false;
    }

    // If |hasShowMoreBeenTapped_| is set to true, never collapse the list.
    if (this.hasShowMoreBeenTapped_) {
      return false;
    }

    // If the total number of saved printers does not exceed the number of
    // visible printers, there is no need for the list to be collapsed.
    if (this.savedPrinters.length - this.visiblePrinterCounter_ < 1) {
      return false;
    }

    return true;
  }

  /**
   * Moves printers that are in |newPrinters_| to position |toIndex| of
   * |printerArr|. This moves all recently added printers to the top of the
   * printer list.
   */
  private moveNewlyAddedPrinters_(
      printerArr: PrinterListEntry[], toIndex: number) {
    if (!this.newPrinters_.length) {
      return;
    }

    // We have newly added printers, move them to the top of the list.
    for (const printer of this.newPrinters_) {
      const idx = printerArr.findIndex(
          p => p.printerInfo.printerId === printer.printerInfo.printerId);
      if (idx > -1) {
        moveEntryInPrinters(printerArr, idx, toIndex);
      }
    }
  }

  private getFilteredPrintersLength_(): number {
    return this.filteredPrinters_.length;
  }

  /** Query each saved printer for its printer status. */
  private fetchPrinterStatuses_(): void {
    if (!this.isPrinterSettingsPrinterStatusEnabled_) {
      return;
    }

    this.savedPrinters.forEach(printer => {
      this.browserProxy_
          .requestPrinterStatusUpdate(printer.printerInfo.printerId)
          .then(printerStatus => this.onPrinterStatusReceived_(printerStatus));
    });
  }

  /**
   * For each printer status received, add it to the printer status cache then
   * notify its respective printer entry to update its status.
   */
  private onPrinterStatusReceived_(printerStatus: PrinterStatus): void {
    assert(this.isPrinterSettingsPrinterStatusEnabled_);
    if (!printerStatus) {
      return;
    }

    this.printerStatusReasonCache_.set(
        printerStatus.printerId,
        getStatusReasonFromPrinterStatus(printerStatus));

    // The actual printer entries displayed are from `filteredPrinters_`. So
    // notify the specific filtered printer entry to update its icon.
    const filteredIndex = this.filteredPrinters_.findIndex(
        printer => printer.printerInfo.printerId === printerStatus.printerId);
    if (filteredIndex === -1) {
      return;
    }

    this.notifyPath(`filteredPrinters_.${filteredIndex}.printerInfo.printerId`);
  }

  /**
   * Starts the printer status query timer which continually resets itself
   * until the page is closed.
   */
  private startPrinterStatusQueryTimer_(): void {
    assert(this.isPrinterSettingsPrinterStatusEnabled_);

    // Chooses a random number between the delay interval.
    const minDelay = this.useShortDelayInterval_ ?
        PRINTER_STATUS_QUERY_SHORT_DELAY_RANGE_MS[0] :
        PRINTER_STATUS_QUERY_LONG_DELAY_RANGE_MS[0];
    const maxDelay = this.useShortDelayInterval_ ?
        PRINTER_STATUS_QUERY_SHORT_DELAY_RANGE_MS[1] :
        PRINTER_STATUS_QUERY_LONG_DELAY_RANGE_MS[1];
    const randomizedDelayMs =
        (Math.random() * (maxDelay - minDelay)) + minDelay;
    setTimeout(
        () => this.onPrinterStatusQueryTimerComplete_(), randomizedDelayMs);
  }

  /**
   * Invoked once the timer is elapsed. Starts the printer status queries then
   * resets the timer.
   */
  private onPrinterStatusQueryTimerComplete_(): void {
    assert(this.isPrinterSettingsPrinterStatusEnabled_);

    this.fetchPrinterStatuses_();

    // Restart the printer status query timer.
    this.startPrinterStatusQueryTimer_();
  }

  startPrinterStatusQueryTimerForTesting(): void {
    this.startPrinterStatusQueryTimer_();
  }

  getPrinterStatusReasonCacheForTesting(): Map<string, PrinterStatusReason> {
    return this.printerStatusReasonCache_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cups-saved-printers': SettingsCupsSavedPrintersElement;
  }
  interface HTMLElementEventMap {
    'open-action-menu':
        CustomEvent<{target: HTMLElement, item: PrinterListEntry}>;
  }
}

customElements.define(
    SettingsCupsSavedPrintersElement.is, SettingsCupsSavedPrintersElement);
