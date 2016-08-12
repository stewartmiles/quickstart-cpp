// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "firebase/admob.h"
#include "firebase/admob/banner_view.h"
#include "firebase/admob/interstitial_ad.h"
#include "firebase/admob/types.h"
#include "firebase/app.h"
#include "firebase/future.h"

// Thin OS abstraction layer.
#include "main.h"  // NOLINT

// A simple listener that logs changes to a BannerView.
class LoggingBannerViewListener : public firebase::admob::BannerView::Listener {
 public:
  LoggingBannerViewListener() {}
  void OnPresentationStateChanged(
      firebase::admob::BannerView* banner_view,
      firebase::admob::BannerView::PresentationState new_state) override {
    ::LogMessage("BannerView PresentationState has changed to %d.", new_state);
  }
  void OnBoundingBoxChanged(firebase::admob::BannerView* banner_view,
                            firebase::admob::BoundingBox new_box) override {
    ::LogMessage(
        "BannerView BoundingBox has changed to (x: %d, y: %d, width: %d, "
        "height %d).",
        new_box.x, new_box.y, new_box.width, new_box.height);
  }
};

// A simple listener that logs changes to an InterstitialAd.
class LoggingInterstitialAdListener
    : public firebase::admob::InterstitialAd::Listener {
 public:
  LoggingInterstitialAdListener() {}
  void OnPresentationStateChanged(
      firebase::admob::InterstitialAd* interstitial_ad,
      firebase::admob::InterstitialAd::PresentationState new_state) override {
    ::LogMessage("InterstitialAd PresentationState has changed to %d.",
                 new_state);
  }
};

// These ad units are configured to always serve test ads.
#if defined(__ANDROID__)
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/6300978111";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/1033173712";
#else
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/2934735716";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/4411468910";
#endif

// Standard mobile banner size is 320x50.
static const int kBannerWidth = 320;
static const int kBannerHeight = 50;

// Sample keywords to use in making the request.
static const char* kKeywords[] = {"AdMob", "C++", "Fun"};

// Sample test device IDs to use in making the request.
static const char* kTestDeviceIDs[] = {"2077ef9a63d2b398840261c8221a0c9b",
                                       "098fe087d987c9a878965454a65654d7"};

// Sample birthday value to use in making the request.
static const int kBirthdayDay = 10;
static const int kBirthdayMonth = 11;
static const int kBirthdayYear = 1976;

static firebase::App* g_app = nullptr;
static firebase::admob::BannerView* g_banner = nullptr;
static firebase::admob::AdRequest g_request;
static LoggingBannerViewListener g_banner_listener;

static void WaitForFutureCompletion(firebase::FutureBase future) {
  while (!ProcessEvents(1000)) {
    if (future.Status() != firebase::kFutureStatusPending) {
      break;
    }
  }

  if (future.Error() != firebase::admob::kAdMobErrorNone) {
    LogMessage("Action failed with error code %d and message \"%s\".",
               future.Error(), future.ErrorMessage());
  }
}

// Initializes Firebase if it's not already initialized.
void InitializeFirebase() {
  if (g_app) return;
#if defined(__ANDROID__)
  g_app = firebase::App::Create(firebase::AppOptions(), GetJniEnv(),
								GetActivity());
#else
  g_app = firebase::App::Create(firebase::AppOptions());
#endif  // defined(__ANDROID__)

  LogMessage("Created the Firebase App %x.",
             static_cast<int>(reinterpret_cast<intptr_t>(g_app)));

  LogMessage("Initializing the AdMob with Firebase API.");
  firebase::admob::Initialize(*g_app);

  // If the app is aware of the user's gender, it can be added to the targeting
  // information. Otherwise, "unknown" should be used.
  g_request.gender = firebase::admob::kGenderUnknown;

  // This value allows publishers to specify whether they would like the request
  // to be treated as child-directed for purposes of the Children’s Online
  // Privacy Protection Act (COPPA).
  // See http://business.ftc.gov/privacy-and-security/childrens-privacy.
  g_request.tagged_for_child_directed_treatment =
      firebase::admob::kChildDirectedTreatmentStateTagged;

  // The user's birthday, if known. Note that months are indexed from one.
  g_request.birthday_day = kBirthdayDay;
  g_request.birthday_month = kBirthdayMonth;
  g_request.birthday_year = kBirthdayYear;

  // Additional keywords to be used in targeting.
  g_request.keyword_count = sizeof(kKeywords) / sizeof(kKeywords[0]);
  g_request.keywords = kKeywords;

  // "Extra" key value pairs can be added to the request as well. Typically
  // these are used when testing new features.
  static const firebase::admob::KeyValuePair kRequestExtras[] = {
      {"the_name_of_an_extra", "the_value_for_that_extra"}};
  g_request.extras_count = sizeof(kRequestExtras) / sizeof(kRequestExtras[0]);
  g_request.extras = kRequestExtras;

  // This example uses ad units that are specially configured to return test ads
  // for every request. When using your own ad unit IDs, however, it's important
  // to register the device IDs associated with any devices that will be used to
  // test the app. This ensures that regardless of the ad unit ID, those
  // devices will always receive test ads in compliance with AdMob policy.
  //
  // Device IDs can be obtained by checking the logcat or the Xcode log while
  // debugging. They appear as a long string of hex characters.
  g_request.test_device_id_count =
      sizeof(kTestDeviceIDs) / sizeof(kTestDeviceIDs[0]);
  g_request.test_device_ids = kTestDeviceIDs;

  // Create an ad size for the BannerView.
  firebase::admob::AdSize ad_size;
  ad_size.ad_size_type = firebase::admob::kAdSizeStandard;
  ad_size.width = kBannerWidth;
  ad_size.height = kBannerHeight;

  LogMessage("Creating the BannerView.");
  g_banner = new firebase::admob::BannerView();
  g_banner->SetListener(&g_banner_listener);
  g_banner->Initialize(GetWindowContext(), kBannerAdUnit, ad_size);

  WaitForFutureCompletion(g_banner->InitializeLastResult());

  // Make the BannerView visible.
  LogMessage("Showing the banner ad.");
  g_banner->Show();

  // Wait for Ad show to complete.
  WaitForFutureCompletion(g_banner->ShowLastResult());

  // When the BannerView is visible, load an ad into it.
  LogMessage("Loading a banner ad.");
  g_banner->LoadAd(g_request);
}

// Execute all methods of the C++ admob API.
extern "C" int common_main(int argc, const char* argv[]) {
  firebase::App* app;
  LogMessage("Initializing the AdMob library.");
  InitializeFirebase();

  // Wait for the load request to complete.
  WaitForFutureCompletion(g_banner->LoadAdLastResult());

  // Move to each of the six pre-defined positions.
  LogMessage("Moving the banner ad to top-center.");
  g_banner->MoveTo(firebase::admob::BannerView::kPositionTop);

  // Move to each of the six pre-defined positions.
  LogMessage("Moving the banner ad to top-center.");
  g_banner->MoveTo(firebase::admob::BannerView::kPositionTop);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Moving the banner ad to top-left.");
  g_banner->MoveTo(firebase::admob::BannerView::kPositionTopLeft);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Moving the banner ad to top-right.");
  g_banner->MoveTo(firebase::admob::BannerView::kPositionTopRight);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Moving the banner ad to bottom-center.");
  g_banner->MoveTo(firebase::admob::BannerView::kPositionBottom);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Moving the banner ad to bottom-left.");
  g_banner->MoveTo(firebase::admob::BannerView::kPositionBottomLeft);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Moving the banner ad to bottom-right.");
  g_banner->MoveTo(firebase::admob::BannerView::kPositionBottomRight);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  // Try some coordinate moves.
  LogMessage("Moving the banner ad to (100, 300).");
  g_banner->MoveTo(100, 300);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Moving the banner ad to (100, 400).");
  g_banner->MoveTo(100, 400);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  // Try hiding and showing the BannerView.
  LogMessage("Hiding the banner ad.");
  g_banner->Hide();

  WaitForFutureCompletion(g_banner->HideLastResult());

  LogMessage("Showing the banner ad.");
  g_banner->Show();

  WaitForFutureCompletion(g_banner->ShowLastResult());

  // A few last moves after showing it again.
  LogMessage("Moving the banner ad to (100, 300).");
  g_banner->MoveTo(100, 300);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Moving the banner ad to (100, 400).");
  g_banner->MoveTo(100, 400);

  WaitForFutureCompletion(g_banner->MoveToLastResult());

  LogMessage("Hiding the banner ad now that we're done with it.");
  g_banner->Hide();

  WaitForFutureCompletion(g_banner->HideLastResult());

  // Create and test InterstitialAd.
  LogMessage("Creating the InterstitialAd.");
  LoggingInterstitialAdListener interstitial_listener;
  firebase::admob::InterstitialAd* interstitial =
      new firebase::admob::InterstitialAd();
  interstitial->SetListener(&interstitial_listener);
  interstitial->Initialize(GetWindowContext(), kInterstitialAdUnit);

  WaitForFutureCompletion(interstitial->InitializeLastResult());

  // When the InterstitialAd is initialized, load an ad.
  LogMessage("Loading an interstitial ad.");
  interstitial->LoadAd(g_request);

  WaitForFutureCompletion(interstitial->LoadAdLastResult());

  // When the InterstitialAd has loaded an ad, show it.
  LogMessage("Showing the interstitial ad.");
  interstitial->Show();

  WaitForFutureCompletion(interstitial->ShowLastResult());

  // Wait for the user to close the interstitial.
  while (interstitial->GetPresentationState() !=
         firebase::admob::InterstitialAd::PresentationState::
             kPresentationStateHidden) {
    ProcessEvents(1000);
  }

  LogMessage("Done!");

  // Wait until the user kills the app.
  while (!ProcessEvents(1000)) {
  }

  delete g_banner;
  g_banner = nullptr;
  delete interstitial;
  firebase::admob::Terminate();
  delete g_app;
  g_app = nullptr;

  return 0;
}
