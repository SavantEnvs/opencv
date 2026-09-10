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
// Mayhem port (savantenvs/opencv, 5.x). The mayhemheroes/opencv 4.x copy had the storage.open()
// call COMMENTED OUT ("enabling the following crashes right away"), i.e. the harness was a no-op
// that only wrote and deleted a temp file. It is re-enabled here, inside the try/catch the original
// authors already wrote — FileStorage reports malformed input via cv::Exception; anything else the
// sanitizers see is a real finding.
//
// Surface: cv::FileStorage::open(<file>, READ) — the file-backed persistence reader (fopen + buffered
// gets() path), which sniffs XML / YAML / JSON from the first bytes and parses the whole document.
// The scratch file comes from mayhem_common.h ($TMPDIR + mkstemp(), no extension, so the ".gz"
// zlib path is not selected — that path needs a name, see filestorage_read_filename_fuzzer).

#include <cstddef>
#include <cstdint>

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

#include "mayhem_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Tests reading from a file using cv::FileStorage, which attempts to parse
  // JSON, XML, and YAML, using the first few bytes of a file to determine which
  // type to parse it as.
  const FuzzerTemporaryFile temp_file(data, size);
  cv::FileStorage storage;
  try {
    storage.open(temp_file.filename(), cv::FileStorage::READ);
  } catch (const cv::Exception&) {
    // Do nothing.
  }
  return 0;
}
