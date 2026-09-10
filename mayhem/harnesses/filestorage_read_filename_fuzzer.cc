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
// Mayhem port (savantenvs/opencv, 5.x): logic unchanged from the mayhemheroes/opencv 4.x layer
// except for ONE narrow hang guard (below).
//
// Surface: cv::FileStorage::open(<name>, READ) file-name handling — analyze_file_name() ("?base64"
// parameter splitting), the extension logic (".gz" / ".gz[0-9]" -> zlib gzopen, else fopen), and
// the not-found error path. The name normally does not exist, so the parser itself is not reached
// (that is filestorage_read_file_fuzzer / filestorage_read_string_fuzzer's job).
//
// Hang guard: the original comment already noted the fuzzer "may actually generate filenames that
// do exist". A name that resolves to an endless or blocking device (/dev/zero, /dev/urandom,
// /dev/stdin, ../../dev/zero, ...) makes open() read forever, and one such input in the corpus
// stalls every future Mayhem run. Every '/' in the name is therefore mapped to '_' so the name can
// only ever denote an entry of the (writable, finite) working directory. Nothing in the name-parsing
// surface above depends on path separators, so no coverage is lost.

#include <cstddef>
#include <cstdint>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

#include "mayhem_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Tests filename parsing when opening cv::FileStorage for reading. The file
  // doesn't actually exist, so any logic prediated on successfully opening the
  // file will not be tested.
  std::string filename(reinterpret_cast<const char*>(data), size);
  for (size_t i = 0; i < filename.size(); ++i) {
    if (filename[i] == '/') filename[i] = '_';
  }
  cv::FileStorage storage;
  try {
    storage.open(filename, cv::FileStorage::READ);
  } catch (const cv::Exception&) {
    // Do nothing.
  }
  return 0;
}
