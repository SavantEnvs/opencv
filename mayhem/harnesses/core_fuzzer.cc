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
// Mayhem port (savantenvs/opencv, 5.x): unchanged logic from the mayhemheroes/opencv 4.x layer.
// Additive changes only — narrow includes (core + imgcodecs instead of opencv.hpp), the shared
// mayhem_common.h runtime limits, and exceptions caught by const reference.
//
// Surface: cv::imdecode of the fuzzer bytes (any codec) -> Mat::clone -> Mat::t()/inv()/diag()
// -> cv::split + cv::merge. Falls back to a 1xN CV_8UC1 view of the raw bytes when decoding fails.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <fuzzer/FuzzedDataProvider.h>

#include "mayhem_common.h"

namespace {

bool GetCVImage(const std::string& image_string, const int max_pixels,
                cv::Mat* original_image) {
  (void)max_pixels;
  if (image_string.empty()) return false;
  std::vector<uchar> raw_data(image_string.size());
  const char* ptr = image_string.data();
  std::copy(ptr, ptr + image_string.size(), raw_data.data());
  try {
    *original_image = cv::imdecode(raw_data, cv::IMREAD_UNCHANGED);
  } catch (const cv::Exception&) {}
  return !original_image->empty();
}

void TestInternalMethods(const cv::Mat& mat) {
  try {
    mat.t();
  } catch (const cv::Exception&) {}
  try {
    mat.inv();
  } catch (const cv::Exception&) {}
  try {
    mat.diag();
  } catch (const cv::Exception&) {}
}

void TestSplitAndMerge(const cv::Mat& image) {
  std::vector<cv::Mat> split_image(image.channels());
  cv::split(image, split_image);
  if (!split_image.empty()) {
    cv::Mat new_image;
    cv::merge(split_image, new_image);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Prepares a backup image we will use if we cannot successfully get an image
  // by decoding the string.
  std::vector<uint8_t> image_data = {data, data + size};
  cv::Mat backup_image =
      cv::Mat(1, static_cast<int>(image_data.size()), CV_8UC1, image_data.data());

  FuzzedDataProvider fuzzed_data_provider(data, size);
  const int max_pixels = fuzzed_data_provider.ConsumeIntegral<int>();
  const std::string image_string =
      fuzzed_data_provider.ConsumeRemainingBytesAsString();
  cv::Mat original_image;
  // Tests the clone method.
  cv::Mat cloned_image = GetCVImage(image_string, max_pixels, &original_image)
                             ? original_image.clone()
                             : backup_image.clone();

  TestInternalMethods(cloned_image);
  TestSplitAndMerge(cloned_image);
  return 0;
}
