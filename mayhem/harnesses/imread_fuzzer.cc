// OpenCV OSS-Fuzz-derived harness (mayhemheroes/opencv 4.x layer), ported to savantenvs/opencv 5.x.
//
// Surface: cv::imread with the default IMREAD_COLOR_BGR flags — the file-based loader (signature
// sniffing via findDecoder(filename), codec setSource(filename) file I/O paths) plus the color
// conversion / EXIF-orientation post-processing that IMREAD_UNCHANGED (imdecode_fuzzer) skips.
// The fuzzer bytes are staged in a harness-owned $TMPDIR mkstemp() file (mayhem_common.h); the
// name carries no extension, so codec selection is by content signature, as in production.

#include <cstddef>
#include <cstdint>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "mayhem_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const FuzzerTemporaryFile file(data, size);
  try {
    cv::Mat matrix = cv::imread(file.filename());
  } catch (const cv::Exception&) {
    // Do nothing.
  }
  return 0;
}
