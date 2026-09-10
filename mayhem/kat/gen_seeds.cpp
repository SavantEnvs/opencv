// mayhem/kat/gen_seeds.cpp — DEVELOPMENT-TIME tool (not built by mayhem/build.sh, not run by test.sh).
//
// Produces the tiny format-valid starter seeds committed under mayhem/{imdecode,imread}_fuzzer/testsuite
// for the codecs that upstream ships no small sample of (BMP, PGM/PPM/PAM/PBM, PFM, HDR, Sun Raster,
// TIFF, WebP, JPEG 2000, 16-bit PNG, RGBA PNG). Kept in-tree so the next porter can regenerate them
// against the clean build:
//   clang++ -std=c++17 -O2 -I<module include dirs> -I build/clean mayhem/kat/gen_seeds.cpp \
//       -Wl,--start-group build/clean/lib/*.a build/clean/3rdparty/lib/*.a -Wl,--end-group -ldl -lm -lpthread -lrt -o /tmp/gen_seeds
//   /tmp/gen_seeds <output-dir>
#include <cstdio>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

static cv::Mat gradient(int rows, int cols, int type) {
  cv::Mat m(rows, cols, type);
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < cols; ++c)
      for (int ch = 0; ch < m.channels(); ++ch) {
        int v = (r * 37 + c * 11 + ch * 101 + (r * c) * 3) & 255;
        switch (m.depth()) {
          case CV_8U:  m.ptr<uchar>(r)[c * m.channels() + ch] = (uchar)v; break;
          case CV_16U: m.ptr<ushort>(r)[c * m.channels() + ch] = (ushort)(v * 257); break;
          case CV_32F: m.ptr<float>(r)[c * m.channels() + ch] = (float)v / 255.0f; break;
          default: break;
        }
      }
  return m;
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s <outdir>\n", argv[0]); return 2; }
  std::string dir = argv[1];
  cv::Mat g3 = gradient(6, 8, CV_8UC3), g1 = gradient(6, 8, CV_8UC1), g4 = gradient(6, 8, CV_8UC4);
  cv::Mat g16 = gradient(5, 7, CV_16UC1), g16c3 = gradient(5, 7, CV_16UC3);
  cv::Mat gf = gradient(4, 5, CV_32FC1), gf3 = gradient(4, 5, CV_32FC3);
  cv::Mat bw = g1 > 127;
  // OpenJPEG needs >= 32 px per side for its default resolution count.
  cv::Mat b3 = gradient(40, 48, CV_8UC3), b16 = gradient(40, 48, CV_16UC1), b16c3 = gradient(40, 48, CV_16UC3);
  struct { const char* name; const cv::Mat* img; std::vector<int> params; } seeds[] = {
    {"grad_8uc3.bmp", &g3, {}}, {"grad_8uc1.bmp", &g1, {}},
    {"grad_8uc3.ppm", &g3, {}}, {"grad_8uc1.pgm", &g1, {}}, {"grad_16uc1.pgm", &g16, {}},
    {"grad_8uc3.pam", &g3, {}}, {"grad_8uc4.pam", &g4, {}}, {"bw.pbm", &bw, {}},
    {"grad_8uc3_ascii.ppm", &g3, {cv::IMWRITE_PXM_BINARY, 0}},
    {"grad_32fc1.pfm", &gf, {}}, {"grad_32fc3.pfm", &gf3, {}},
    {"grad_32fc3.hdr", &gf3, {}}, {"grad_32fc3_rle.hdr", &gf3, {cv::IMWRITE_HDR_COMPRESSION, cv::IMWRITE_HDR_COMPRESSION_RLE}},
    {"grad_8uc3.ras", &g3, {}}, {"grad_8uc1.ras", &g1, {}},
    {"grad_8uc3_lzw.tiff", &g3, {}}, {"grad_16uc1.tiff", &g16, {}}, {"grad_32fc1.tiff", &gf, {}},
    {"grad_8uc3_none.tiff", &g3, {cv::IMWRITE_TIFF_COMPRESSION, cv::IMWRITE_TIFF_COMPRESSION_NONE}},
    {"grad_8uc3_deflate.tiff", &g3, {cv::IMWRITE_TIFF_COMPRESSION, cv::IMWRITE_TIFF_COMPRESSION_ADOBE_DEFLATE}},
    {"grad_8uc3_lossless.webp", &g3, {}}, {"grad_8uc3_q60.webp", &g3, {cv::IMWRITE_WEBP_QUALITY, 60}},
    {"grad_8uc4.webp", &g4, {}},
    {"grad_8uc3.jp2", &b3, {}}, {"grad_16uc1.jp2", &b16, {}}, {"grad_16uc3.jp2", &b16c3, {}},
    {"grad_8uc3_lossy.jp2", &b3, {cv::IMWRITE_JPEG2000_COMPRESSION_X1000, 200}},
    {"grad_16uc1.png", &g16, {}}, {"grad_8uc4.png", &g4, {}}, {"grad_8uc1.png", &g1, {}},
    {"grad_8uc3_q50.jpg", &g3, {cv::IMWRITE_JPEG_QUALITY, 50}}, {"grad_8uc1_prog.jpg", &g1, {cv::IMWRITE_JPEG_PROGRESSIVE, 1}},
    {"grad_8uc3.gif", &g3, {}},
  };
  int n = 0;
  for (const auto& s : seeds) {
    std::string path = dir + "/" + s.name;
    bool ok = false;
    try { ok = cv::imwrite(path, *s.img, s.params); } catch (const cv::Exception& e) { fprintf(stderr, "%s: %s\n", s.name, e.what()); }
    printf("%s %s\n", ok ? "wrote" : "FAILED", path.c_str());
    n += ok;
  }
  printf("%d seeds\n", n);
  return 0;
}
