// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/promo_handler.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/companion/core/signin_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace companion {

PromoHandler::PromoHandler(PrefService* pref_service,
                           SigninDelegate* signin_delegate)
    : pref_service_(pref_service), signin_delegate_(signin_delegate) {}

PromoHandler::~PromoHandler() = default;

// static
void PromoHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kMsbbPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kSigninPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kExpsPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kExpsPromoShownCountPref, 0);
  // TODO(shaktisahu): Move the pref registration to a better location.
  registry->RegisterBooleanPref(kExpsOptInStatusGrantedPref, false);
  registry->RegisterBooleanPref(kHasNavigatedToExpsSuccessPage, false);
}

void PromoHandler::OnPromoAction(PromoType promo_type,
                                 PromoAction promo_action) {
  switch (promo_type) {
    case PromoType::kSignin:
      OnSigninPromo(promo_action);
      return;
    case PromoType::kMsbb:
      OnMsbbPromo(promo_action);
      return;
    case PromoType::kExps:
      OnExpsPromo(promo_action);
      return;
    default:
      return;
  }
}

void PromoHandler::OnSigninPromo(PromoAction promo_action) {
  switch (promo_action) {
    case PromoAction::kRejected:
      IncrementPref(kSigninPromoDeclinedCountPref);
      return;
    case PromoAction::kShown:
      return;
    case PromoAction::kAccepted:
      signin_delegate_->StartSigninFlow();
      return;
  }
}

void PromoHandler::OnMsbbPromo(PromoAction promo_action) {
  if (promo_action == PromoAction::kRejected) {
    IncrementPref(kMsbbPromoDeclinedCountPref);
  } else if (promo_action == PromoAction::kAccepted) {
    // Turn on MSBB.
    signin_delegate_->EnableMsbb(true);
  }
}

void PromoHandler::OnExpsPromo(PromoAction promo_action) {
  if (promo_action == PromoAction::kShown) {
    IncrementPref(kExpsPromoShownCountPref);
  } else if (promo_action == PromoAction::kRejected) {
    IncrementPref(kExpsPromoDeclinedCountPref);
  }
}

void PromoHandler::IncrementPref(const std::string& pref_name) {
  int current_val = pref_service_->GetInteger(pref_name);
  pref_service_->SetInteger(pref_name, current_val + 1);
}

}  // namespace companion
