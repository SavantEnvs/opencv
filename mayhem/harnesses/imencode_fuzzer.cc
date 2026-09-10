// Copyright 2020 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Mayhem port (savantenvs/opencv, 5.x) — ADDITIVE enhancement over the mayhemheroes/opencv 4.x
// harness, which only ever encoded the raw bytes as a 1xN CV_8UC1 row to ".tiff":
//   * the LAST byte picks the output codec from kExts (FuzzedDataProvider consumes integrals from
//     the end of the input; index = byte % 14; an exhausted provider yields kExts[0] = ".tiff", so
//     an empty input reproduces the legacy behavior);
//   * the raw 1xN row is encoded exactly as before (legacy path, kept for parity);
//   * the remaining bytes are additionally cv::imdecode'd and, when they form a real image, that
//     decoded Mat (any depth/channel count the input codec produced) is re-encoded — the only way to
//     reach the encoders' multi-row / multi-channel / 16-bit / float paths with fuzzer-shaped data.
// Every OpenCV failure surfaces as cv::Exception and is swallowed; memory-safety and UB findings
// come from the sanitizers.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fuzzer/FuzzedDataProvider.h>

#include "mayhem_common.h"

namespace {

const char* const kExts[] = {".tiff", ".png", ".jpg", ".bmp", ".webp", ".pgm", ".ppm",
                             ".pam",  ".pbm", ".pfm", ".hdr", ".ras",  ".jp2", ".gif"};

void Encode(const char* ext, const cv::Mat& img) {
  try {
    std::vector<uchar> buffer;
    cv::imencode(ext, img, buffer);
  } catch (const cv::Exception&) {
    // Do nothing.
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  const char* ext = fuzzed_data_provider.PickValueInArray(kExts);
  std::vector<uint8_t> image_data =
      fuzzed_data_provider.ConsumeRemainingBytes<uint8_t>();
  cv::Mat data_matrix =
      cv::Mat(1, static_cast<int>(image_data.size()), CV_8UC1, image_data.data());

  // Legacy path: the raw bytes as a single 8-bit row.
  Encode(ext, data_matrix);

  // Decode -> re-encode path: a real image (any type) through the chosen encoder.
  cv::Mat decoded;
  try {
    decoded = cv::imdecode(data_matrix, cv::IMREAD_UNCHANGED);
  } catch (const cv::Exception&) {
    // Do nothing.
  }
  if (!decoded.empty()) Encode(ext, decoded);
  return 0;
}
