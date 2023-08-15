// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"

#include "base/barrier_callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

namespace {

// This enum must match the numbering for NTPHistoryClustersIneligibleReason in
// enums.xml. Do not reorder or remove items, and update kMaxValue when new
// items are added.
enum NTPHistoryClustersIneligibleReason {
  kNone = 0,
  kNoClusters = 1,
  kNonProminent = 2,
  kNoSRPVisit = 3,
  kInsufficientVisits = 4,
  kInsufficientImages = 5,
  kInsufficientRelatedSearches = 6,
  kMaxValue = kInsufficientRelatedSearches,
};

base::Time GetBeginTime() {
  static int hours_to_look_back = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleBeginTimeDuration,
      ntp_features::kNtpHistoryClustersModuleBeginTimeDurationHoursParam, 24);
  if (hours_to_look_back <= 0) {
    hours_to_look_back = 24;
  }

  return base::Time::Now() - base::Hours(hours_to_look_back);
}

}  // namespace

HistoryClustersModuleService::HistoryClustersModuleService(
    history_clusters::HistoryClustersService* history_clusters_service,
    CartService* cart_service,
    TemplateURLService* template_url_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : max_clusters_to_return_(GetMaxClusters()),
      category_boostlist_(GetCategories(
          ntp_features::kNtpHistoryClustersModuleCategoriesBoostlistParam)),
      should_fetch_clusters_until_exhausted_(base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleFetchClustersUntilExhausted)),
      history_clusters_service_(history_clusters_service),
      cart_service_(cart_service),
      template_url_service_(template_url_service) {
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleUseModelRanking) &&
      optimization_guide_keyed_service) {
    module_ranker_ = std::make_unique<HistoryClustersModuleRanker>(
        optimization_guide_keyed_service, cart_service_, category_boostlist_);
  }
}
HistoryClustersModuleService::~HistoryClustersModuleService() = default;

void HistoryClustersModuleService::GetClusters(
    const history_clusters::QueryClustersFilterParams filter_params,
    size_t min_required_related_searches,
    GetClustersCallback callback) {
  if (!history_clusters_service_->IsJourneysEnabledAndVisible()) {
    std::move(callback).Run({}, {});
    return;
  }

  if (!template_url_service_) {
    std::move(callback).Run({}, {});
    return;
  }

  GetClusters(GetBeginTime(), std::move(filter_params),
              min_required_related_searches,
              history_clusters::QueryClustersContinuationParams(), {},
              std::move(callback));
}

void HistoryClustersModuleService::GetClusters(
    base::Time begin_time,
    const history_clusters::QueryClustersFilterParams filter_params,
    size_t min_required_related_searches,
    history_clusters::QueryClustersContinuationParams continuation_params,
    std::vector<history::Cluster> continuation_clusters,
    GetClustersCallback callback) {
  // TODO(crbug/1442619): Encapsulate work done by this method in a task that
  // gets returned to the caller.

  size_t task_id = task_id_++;
  std::unique_ptr<history_clusters::HistoryClustersServiceTask>
      query_clusters_task = history_clusters_service_->QueryClusters(
          history_clusters::ClusteringRequestSource::kNewTabPage, filter_params,
          begin_time, continuation_params,
          /*recluster=*/false,
          base::BindOnce(&HistoryClustersModuleService::OnGetFilteredClusters,
                         weak_ptr_factory_.GetWeakPtr(), task_id, begin_time,
                         filter_params, min_required_related_searches,
                         std::move(continuation_clusters),
                         std::move(callback)));
  in_progress_query_clusters_tasks_.insert_or_assign(
      task_id, std::move(query_clusters_task));
}

void HistoryClustersModuleService::OnGetFilteredClusters(
    size_t pending_task_id,
    base::Time begin_time,
    history_clusters::QueryClustersFilterParams filter_params,
    size_t min_required_related_searches,
    std::vector<history::Cluster> continuation_clusters,
    GetClustersCallback callback,
    std::vector<history::Cluster> clusters,
    history_clusters::QueryClustersContinuationParams continuation_params) {
  in_progress_query_clusters_tasks_.erase(pending_task_id);

  if (!continuation_clusters.empty()) {
    clusters.insert(clusters.begin(), continuation_clusters.begin(),
                    continuation_clusters.end());
  }

  bool should_fetch_more_clusters = should_fetch_clusters_until_exhausted_ &&
                                    !continuation_params.exhausted_all_visits;
  if (should_fetch_more_clusters) {
    GetClusters(begin_time, std::move(filter_params),
                min_required_related_searches, continuation_params,
                std::move(clusters), std::move(callback));

    return;
  }

  // Within each cluster, sort visits.
  for (auto& cluster : clusters) {
    history_clusters::StableSortVisits(cluster.visits);
  }

  // Do additional filtering on clusters.
  history_clusters::CoalesceRelatedSearches(clusters);

  // Cull clusters that do not have the minimum number of visits with and
  // without images to be eligible for display.
  NTPHistoryClustersIneligibleReason ineligible_reason =
      clusters.empty() ? kNoClusters : kNone;
  base::EraseIf(clusters, [&](auto& cluster) {
    // Cull non prominent clusters.
    if (!cluster.should_show_on_prominent_ui_surfaces) {
      ineligible_reason = kNonProminent;
      return true;
    }

    // Cull clusters whose visits don't have at least one SRP.
    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();
    auto srp_visits_it = std::find_if(
        cluster.visits.begin(), cluster.visits.end(), [&](auto& visit) {
          return default_search_provider->IsSearchURL(
              visit.normalized_url, template_url_service_->search_terms_data());
        });
    if (srp_visits_it == cluster.visits.end()) {
      ineligible_reason = kNoSRPVisit;
      return true;
    }

    // Ensure visits contains at most one SRP visit and its the first one in the
    // list.
    history::ClusterVisit first_srp_visit = *srp_visits_it;
    base::EraseIf(cluster.visits, [&](auto& visit) {
      return default_search_provider->IsSearchURL(
          visit.normalized_url, template_url_service_->search_terms_data());
    });
    cluster.visits.insert(cluster.visits.begin(), first_srp_visit);

    // Cull visits that have a zero relevance score, are Hidden, or Done.
    base::EraseIf(cluster.visits, [&](auto& visit) {
      return visit.score == 0.0 ||
             visit.interaction_state ==
                 history::ClusterVisit::InteractionState::kHidden ||
             visit.interaction_state ==
                 history::ClusterVisit::InteractionState::kDone;
    });

    if (cluster.visits.size() < static_cast<size_t>(filter_params.min_visits)) {
      ineligible_reason = kInsufficientVisits;
      return true;
    }

    int visits_with_images = std::accumulate(
        cluster.visits.begin(), cluster.visits.end(), 0,
        [](const auto& i, const auto& v) {
          return i + int(v.annotated_visit.content_annotations
                             .has_url_keyed_image &&
                         v.annotated_visit.visit_row.is_known_to_sync);
        });
    if (visits_with_images < filter_params.min_visits_with_images) {
      ineligible_reason = kInsufficientImages;
      return true;
    }

    // Cull clusters that do not have the minimum required number of related
    // searches to be eligible for display.
    if (cluster.related_searches.size() < min_required_related_searches) {
      ineligible_reason = kInsufficientRelatedSearches;
      return true;
    }

    return false;
  });

  // Only record metrics if we are ready to rank clusters.
  base::UmaHistogramEnumeration("NewTabPage.HistoryClusters.IneligibleReason",
                                ineligible_reason);
  base::UmaHistogramBoolean("NewTabPage.HistoryClusters.HasClusterToShow",
                            !clusters.empty());
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumClusterCandidates",
                              clusters.size());

  if (clusters.empty()) {
    std::move(callback).Run(/*clusters=*/{}, /*ranking_signals=*/{});
    return;
  }

  if (module_ranker_) {
    module_ranker_->RankClusters(
        std::move(clusters),
        base::BindOnce(&HistoryClustersModuleService::OnGetRankedClusters,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    SortClustersUsingHeuristic(category_boostlist_, clusters);
    OnGetRankedClusters(std::move(callback), std::move(clusters),
                        /*ranking_signals=*/{});
  }
}

void HistoryClustersModuleService::OnGetRankedClusters(
    GetClustersCallback callback,
    std::vector<history::Cluster> clusters,
    base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
        ranking_signals) {
  // Record metrics for top cluster.
  history::Cluster top_cluster = clusters.front();
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumVisits",
                              top_cluster.visits.size());
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumRelatedSearches",
                              top_cluster.related_searches.size());

  // Cull to max clusters to return.
  if (clusters.size() > max_clusters_to_return_) {
    clusters.resize(max_clusters_to_return_);
  }

  std::move(callback).Run(std::move(clusters), std::move(ranking_signals));

  if (!IsCartModuleEnabled() || !cart_service_) {
    return;
  }
  const auto metrics_callback = base::BarrierCallback<bool>(
      top_cluster.visits.size(),
      base::BindOnce([](const std::vector<bool>& results) {
        bool has_cart = false;
        for (bool result : results) {
          has_cart = has_cart || result;
        }
        base::UmaHistogramBoolean(
            "NewTabPage.HistoryClusters.HasCartForTopCluster", has_cart);
      }));
  for (auto& visit : top_cluster.visits) {
    cart_service_->HasActiveCartForURL(visit.normalized_url, metrics_callback);
  }
}
