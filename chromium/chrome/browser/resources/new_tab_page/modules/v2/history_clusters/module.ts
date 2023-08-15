// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cart/cart_tile.js';
import './header_tile.js';
import './visit_tile.js';
import './suggest_tile.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cart} from '../../../cart.mojom-webui.js';
import {Cluster, InteractionState} from '../../../history_cluster_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {NewTabPageProxy} from '../../../new_tab_page_proxy.js';
import {InfoDialogElement} from '../../info_dialog';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {HistoryClustersProxyImpl} from './history_clusters_proxy.js';
import {getTemplate} from './module.html.js';

export const MAX_MODULE_ELEMENT_INSTANCES = 3;

const CLUSTER_MIN_REQUIRED_URL_VISITS = 3;

export interface HistoryClustersModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
  };
}

export class HistoryClustersModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters-redesigned';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      layoutType: Number,

      /** The cart displayed by this element, could be null. */
      cart: {
        type: Object,
        value: null,
      },

      /** The cluster displayed by this element. */
      cluster: {
        type: Object,
      },

      format: {
        type: String,
        reflectToAttribute: true,
      },

      imagesEnabled_: {
        type: Boolean,
        value: true,
        computed: `computeImagesEnabled_(cart)`,
        reflectToAttribute: true,
      },

      showRelatedSearches: {
        type: Boolean,
        computed: `computeShowRelatedSearches()`,
        reflectToAttribute: true,
      },
    };
  }

  cart: Cart|null;
  cluster: Cluster;
  format: string;
  showRelatedSearches: boolean;
  private imagesEnabled_: boolean;
  private setDisabledModulesListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    if (loadTimeData.getBoolean(
            'modulesChromeCartInHistoryClustersModuleEnabled')) {
      this.setDisabledModulesListenerId_ =
          NewTabPageProxy.getInstance()
              .callbackRouter.setDisabledModules.addListener(
                  async (_: boolean, ids: string[]) => {
                    if (ids.includes('chrome_cart')) {
                      this.cart = null;
                    } else if (!this.cart) {
                      const {cart} =
                          await HistoryClustersProxyImpl.getInstance()
                              .handler.getCartForCluster(this.cluster);
                      this.cart = cart;
                    }
                  });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.setDisabledModulesListenerId_) {
      NewTabPageProxy.getInstance().callbackRouter.removeListener(
          this.setDisabledModulesListenerId_);
    }
  }

  private onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesJourneysSentence2')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onDismissButtonClick_() {
    HistoryClustersProxyImpl.getInstance()
        .handler.updateClusterVisitsInteractionState(
            this.cluster.visits, InteractionState.kHidden);
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage', this.cluster.label),
        restoreCallback: () => {
          HistoryClustersProxyImpl.getInstance()
              .handler.updateClusterVisitsInteractionState(
                  this.cluster.visits, InteractionState.kDefault);
        },
      },
    }));
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onShowAllClick_() {
    assert(this.cluster.label.length >= 2);
    HistoryClustersProxyImpl.getInstance().handler.showJourneysSidePanel(
        this.cluster.label.substring(1, this.cluster.label.length - 1));
  }

  private shouldShowCartTile_(cart: Object): boolean {
    return loadTimeData.getBoolean(
               'modulesChromeCartInHistoryClustersModuleEnabled') &&
        !!cart;
  }

  private computeImagesEnabled_(): boolean {
    return loadTimeData.getBoolean('historyClustersImagesEnabled') ||
        !!this.cart;
  }

  private computeShowRelatedSearches(): boolean {
    return this.cluster.relatedSearches.length > 1;
  }

  private computeLabel_(): string {
    return this.cluster.label.replace(/[“”]+/g, '');
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

async function createElement(cluster: Cluster):
    Promise<HistoryClustersModuleElement> {
  const element = new HistoryClustersModuleElement();
  element.cluster = cluster;
  if (loadTimeData.getBoolean(
          'modulesChromeCartInHistoryClustersModuleEnabled')) {
    const {cart} =
        await HistoryClustersProxyImpl.getInstance().handler.getCartForCluster(
            cluster);
    element.cart = cart;
  }

  return element;
}

async function createElements(): Promise<HTMLElement[]|null> {
  const {clusters} =
      await HistoryClustersProxyImpl.getInstance().handler.getClusters();
  if (!clusters || clusters.length === 0) {
    return null;
  }

  const elements: HistoryClustersModuleElement[] = [];
  for (let i = 0; i < clusters.length; i++) {
    if (elements.length === MAX_MODULE_ELEMENT_INSTANCES) {
      break;
    }
    if (clusters[i].visits.length >= CLUSTER_MIN_REQUIRED_URL_VISITS) {
      elements.push(await createElement(clusters[i]));
    }
  }

  return (elements as unknown) as HTMLElement[];
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history_clusters', createElements);
