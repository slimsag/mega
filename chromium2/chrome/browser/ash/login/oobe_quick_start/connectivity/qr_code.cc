// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"

#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "url/url_util.h"

namespace ash::quick_start {

namespace {

// The target device's device type. 7 = CHROME. Values come from this enum:
// http://google3/java/com/google/android/gmscore/integ/client/smartdevice/src/com/google/android/gms/smartdevice/d2d/DeviceType.java;l=57;rcl=526500829
constexpr char kDeviceTypeQueryParamValue[] = "7";

}  // namespace

QRCode::QRCode(RandomSessionId random_session_id, SharedSecret shared_secret)
    : random_session_id_(random_session_id), shared_secret_(shared_secret) {
  GeneratePixelData();
}

QRCode::QRCode(const QRCode& other) = default;

QRCode& QRCode::operator=(const QRCode& other) = default;

QRCode::~QRCode() = default;

void QRCode::GeneratePixelData() {
  std::vector<uint8_t> blob = GetQRCodeData();
  qr_code_generator::QRCodeGenerator qr_generator;
  auto generated_code = qr_generator.Generate(
      base::as_bytes(base::make_span(blob.data(), blob.size())));
  CHECK(generated_code.has_value());
  auto res =
      PixelData{generated_code->data.begin(), generated_code->data.end()};
  CHECK_EQ(res.size(), static_cast<size_t>(generated_code->qr_size *
                                           generated_code->qr_size));
  pixel_data_ = res;
}

std::vector<uint8_t> QRCode::GetQRCodeData() {
  std::string shared_secret_str(shared_secret_.begin(), shared_secret_.end());
  std::string shared_secret_base64;
  base::Base64Encode(shared_secret_str, &shared_secret_base64);
  url::RawCanonOutputT<char> shared_secret_base64_uriencoded;
  url::EncodeURIComponent(shared_secret_base64.data(),
                          shared_secret_base64.size(),
                          &shared_secret_base64_uriencoded);

  std::string url = "https://signin.google/qs/" +
                    random_session_id_.ToString() + "?key=" +
                    std::string(shared_secret_base64_uriencoded.data(),
                                shared_secret_base64_uriencoded.length()) +
                    "&t=" + std::string(kDeviceTypeQueryParamValue);

  return std::vector<uint8_t>(url.begin(), url.end());
}

}  // namespace ash::quick_start
