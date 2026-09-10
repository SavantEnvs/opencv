// OpenCV OSS-Fuzz-derived harness (mayhemheroes/opencv 4.x layer), ported to savantenvs/opencv 5.x.
//
// 5.x API fix: <opencv2/imgcodecs/legacy/constants_c.h> and CV_LOAD_IMAGE_UNCHANGED no longer
// exist — replaced by cv::IMREAD_UNCHANGED (same value, -1; no color conversion, alpha kept, EXIF
// orientation ignored). The IMREAD_COLOR conversion path is covered by imread_fuzzer.
//
// Surface: cv::imdecode over every compiled-in codec (PNG/libpng, JPEG/libjpeg-turbo, TIFF/libtiff,
// WebP/libwebp, JPEG 2000/OpenJPEG, BMP, PNM/PAM, PFM, HDR, Sun Raster, GIF), all built from the
// vendored 3rdparty sources with the same sanitizer + coverage instrumentation as OpenCV itself.

#include <cstddef>
#include <cstdint>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "mayhem_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> image_data = {data, data + size};
  cv::Mat data_matrix =
      cv::Mat(1, static_cast<int>(image_data.size()), CV_8UC1, image_data.data());
  try {
    cv::Mat decoded_matrix = cv::imdecode(data_matrix, cv::IMREAD_UNCHANGED);
  } catch (const cv::Exception&) {
    // Do nothing.
  }
  return 0;
}
