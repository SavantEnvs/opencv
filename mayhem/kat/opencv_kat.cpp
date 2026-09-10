// mayhem/kat/opencv_kat.cpp — known-answer probe for the behavioral oracle (mayhem/test.sh).
//
// Built by mayhem/build.sh with the project's NORMAL flags against the clean (unsanitized) static
// libraries, as a DYNAMICALLY linked executable (/mayhem/opencv_kat), so the verify-repo sabotage
// check (LD_PRELOAD neuter -> _exit(0)) reaches it: neutered, it prints nothing, every assertion in
// test.sh misses, and the oracle fails — which is the behavioral property the gate requires.
//
// Usage: opencv_kat <templ.png>   (the committed copy of samples/data/templ.png, mayhem/kat/templ.png)
// Output: one KEY=VALUE line per computed fact. test.sh greps each expected line EXACTLY. The values
// were recorded ONCE from the clean build of the committed tree; they are functions of the library's
// behavior only (decoded pixels, encoder output, parsed persistence values, matrix algebra,
// glyph rasterization, color conversion, resampling), never of the environment.
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

uint64_t fnv1a64(const std::string& s) {
  uint64_t h = 1469598103934665603ULL;
  for (unsigned char c : s) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  return h;
}

// Every element of every channel, row-major, as an integer (floating types rounded after scaling).
std::string join(const cv::Mat& m, double scale = 1.0) {
  cv::Mat flat = m.isContinuous() ? m.reshape(1, 1) : m.clone().reshape(1, 1);
  std::string out;
  char buf[64];
  for (int i = 0; i < flat.cols; ++i) {
    double v = 0;
    switch (flat.depth()) {
      case CV_8U:  v = flat.at<uchar>(0, i); break;
      case CV_8S:  v = flat.at<schar>(0, i); break;
      case CV_16U: v = flat.at<ushort>(0, i); break;
      case CV_16S: v = flat.at<short>(0, i); break;
      case CV_32S: v = flat.at<int>(0, i); break;
      case CV_32F: v = flat.at<float>(0, i); break;
      case CV_64F: v = flat.at<double>(0, i); break;
      default: v = -1; break;
    }
    snprintf(buf, sizeof(buf), "%s%d", i ? "," : "", cvRound(v * scale));
    out += buf;
  }
  return out;
}

void shape(const char* key, const cv::Mat& m) {
  printf("%s_SHAPE=%dx%dx%d/%d\n", key, m.rows, m.cols, m.channels(), m.depth());
}

void sum4(const char* key, const cv::Mat& m) {
  cv::Scalar s = cv::sum(m);
  printf("%s_SUM=%.0f,%.0f,%.0f,%.0f\n", key, s[0], s[1], s[2], s[3]);
}

bool same(const cv::Mat& a, const cv::Mat& b) {
  if (a.size() != b.size() || a.type() != b.type()) return false;
  cv::Mat d;
  cv::absdiff(a, b, d);
  return cv::countNonZero(d.reshape(1)) == 0;
}

// imencode(ext) -> imdecode(IMREAD_UNCHANGED) round trip of a synthetic image. RT_<key>=OK means the
// decoded pixels are bit-identical; ENC_<key>_LEN is printed for the uncompressed container formats
// whose length is fixed by the format (header + raw samples).
void roundtrip(const char* key, const char* ext, const cv::Mat& src, bool print_len) {
  std::vector<uchar> buf;
  bool ok = false;
  try {
    ok = cv::imencode(ext, src, buf);
  } catch (const cv::Exception& e) {
    printf("RT_%s=ENCERR\n", key);
    return;
  }
  if (!ok) {
    printf("RT_%s=ENCFAIL\n", key);
    return;
  }
  if (print_len) printf("ENC_%s_LEN=%zu\n", key, buf.size());
  cv::Mat dec;
  try {
    dec = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
  } catch (const cv::Exception& e) {
    printf("RT_%s=DECERR\n", key);
    return;
  }
  printf("RT_%s=%s\n", key, dec.empty() ? "DECFAIL" : same(dec, src) ? "OK" : "DIFF");
  shape((std::string("RT_") + key).c_str(), dec);
  // Exact decoded content (for lossy codec pairs this — not OK/DIFF — is the asserted known answer).
  if (!dec.empty())
    printf("RT_%s_FNV=%016" PRIx64 "\n", key,
           fnv1a64(std::string((const char*)dec.data, dec.total() * dec.elemSize())));
}

cv::Mat gradient(int rows, int cols, int type) {
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

void filestorage(const char* key, int fmt) {
  cv::FileStorage w;
  w.open(".mem", cv::FileStorage::WRITE | cv::FileStorage::MEMORY | fmt);
  w << "a" << 42 << "d" << 3.5 << "s" << "mayhem";
  w << "m" << cv::Mat_<int>({2, 2}, {1, 2, 3, 4});
  w << "seq" << "[" << 1 << 2 << 3 << "]";
  w << "map" << "{" << "x" << 7 << "y" << "eight" << "}";
  std::string txt = w.releaseAndGetString();
  printf("FS_%s_LEN=%zu\n", key, txt.size());
  printf("FS_%s_FNV=%016" PRIx64 "\n", key, fnv1a64(txt));

  cv::FileStorage r(txt, cv::FileStorage::READ | cv::FileStorage::MEMORY);
  int a = -1, x = -1;
  double d = 0;
  std::string s, y;
  cv::Mat m;
  std::vector<int> seq;
  r["a"] >> a;
  r["d"] >> d;
  r["s"] >> s;
  r["m"] >> m;
  r["seq"] >> seq;
  r["map"]["x"] >> x;
  r["map"]["y"] >> y;
  printf("FS_%s_A=%d\nFS_%s_D=%g\nFS_%s_S=%s\nFS_%s_M=%s\n", key, a, key, d, key, s.c_str(), key,
         join(m).c_str());
  printf("FS_%s_SEQ=%s\nFS_%s_MAP=%d,%s\nFS_%s_FORMAT=%d\n", key, join(cv::Mat(seq)).c_str(), key, x,
         y.c_str(), key, r.getFormat());
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <templ.png>\n", argv[0]);
    return 2;
  }

  // ---- imgcodecs: decode the committed PNG fixture three ways --------------------------------
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uchar> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  printf("PNG_BYTES=%zu\n", bytes.size());

  cv::Mat img = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
  shape("PNG", img);
  sum4("PNG", img);
  if (!img.empty()) {
    printf("PNG_PIX_0_0=%s\n", join(img(cv::Rect(0, 0, 1, 1))).c_str());
    printf("PNG_PIX_MID=%s\n", join(img(cv::Rect(img.cols / 2, img.rows / 2, 1, 1))).c_str());
    printf("PNG_PIX_LAST=%s\n", join(img(cv::Rect(img.cols - 1, img.rows - 1, 1, 1))).c_str());
    printf("PNG_ROW0_FNV=%016" PRIx64 "\n",
           fnv1a64(std::string((const char*)img.ptr(0), img.cols * img.elemSize())));
  }
  cv::Mat gray = cv::imdecode(bytes, cv::IMREAD_GRAYSCALE);
  shape("PNG_GRAY", gray);
  sum4("PNG_GRAY", gray);
  cv::Mat rd = cv::imread(argv[1], cv::IMREAD_COLOR);
  shape("IMREAD", rd);
  sum4("IMREAD", rd);
  cv::Mat half = cv::imread(argv[1], cv::IMREAD_REDUCED_COLOR_2);
  shape("IMREAD_HALF", half);
  sum4("IMREAD_HALF", half);

  // ---- imgcodecs: encoder/decoder round trips ------------------------------------------------
  cv::Mat g3 = gradient(6, 8, CV_8UC3), g1 = gradient(6, 8, CV_8UC1);
  cv::Mat g16 = gradient(5, 7, CV_16UC1), gf = gradient(4, 5, CV_32FC1), gf3 = gradient(4, 5, CV_32FC3);
  cv::Mat g4 = gradient(6, 8, CV_8UC4);
  printf("G3_FNV=%016" PRIx64 "\n", fnv1a64(std::string((const char*)g3.data, g3.total() * g3.elemSize())));
  roundtrip("PNG8UC3", ".png", g3, false);
  roundtrip("PNG8UC1", ".png", g1, false);
  roundtrip("PNG16UC1", ".png", g16, false);
  roundtrip("PNG8UC4", ".png", g4, false);
  roundtrip("BMP8UC3", ".bmp", g3, true);
  roundtrip("BMP8UC1", ".bmp", g1, true);
  roundtrip("TIFF8UC3", ".tiff", g3, false);
  roundtrip("TIFF16UC1", ".tiff", g16, false);
  roundtrip("TIFF32FC1", ".tiff", gf, false);
  roundtrip("WEBP8UC3", ".webp", g3, false);
  roundtrip("WEBP8UC4", ".webp", g4, false);
  roundtrip("PPM8UC3", ".ppm", g3, true);
  roundtrip("PGM8UC1", ".pgm", g1, true);
  roundtrip("PGM16UC1", ".pgm", g16, true);
  roundtrip("PAM8UC3", ".pam", g3, true);
  roundtrip("PFM32FC1", ".pfm", gf, true);
  roundtrip("PFM32FC3", ".pfm", gf3, true);
  roundtrip("RAS8UC3", ".ras", g3, true);
  {  // Sun Raster gray: the encoder/decoder pair is not pixel-identical upstream; assert its exact output.
    std::vector<uchar> rb;
    cv::imencode(".ras", g1, rb);
    printf("ENC_RAS8UC1_LEN=%zu\n", rb.size());
    cv::Mat rd1 = cv::imdecode(rb, cv::IMREAD_UNCHANGED);
    shape("RAS8UC1", rd1);
    printf("RAS8UC1_FNV=%016" PRIx64 "\n",
           fnv1a64(std::string((const char*)rd1.data, rd1.total() * rd1.elemSize())));
  }
  // OpenJPEG needs >= 32 px per side for its default 6 resolution levels; use 40x48 images here.
  cv::Mat b3 = gradient(40, 48, CV_8UC3), b16 = gradient(40, 48, CV_16UC1);
  roundtrip("JP28UC3", ".jp2", b3, false);
  roundtrip("JP216UC1", ".jp2", b16, false);
  // Radiance HDR is RGBE (lossy) and GIF palettizes: for those assert the decoded shape + a checksum of
  // what the codec pair produces, and an exact round trip only for a 2-color GIF (palette-exact).
  {
    std::vector<uchar> hb;
    cv::imencode(".hdr", gf3, hb);
    cv::Mat hd = cv::imdecode(hb, cv::IMREAD_UNCHANGED);
    shape("HDR32FC3", hd);
    printf("HDR32FC3_FNV=%016" PRIx64 "\n",
           fnv1a64(std::string((const char*)hd.data, hd.total() * hd.elemSize())));
  }
  cv::Mat two = g3.clone();
  for (int r = 0; r < two.rows; ++r)
    for (int c = 0; c < two.cols; ++c) two.at<cv::Vec3b>(r, c) = ((r + c) & 1) ? cv::Vec3b(255, 255, 255) : cv::Vec3b(0, 0, 0);
  roundtrip("GIF2COLOR", ".gif", two, false);
  {
    std::vector<uchar> gb;
    cv::imencode(".gif", two, gb);
    cv::Mat gd = cv::imdecode(gb, cv::IMREAD_UNCHANGED);
    printf("GIF2COLOR_ENC_LEN=%zu\nGIF2COLOR_SRC_ROW0=%s\nGIF2COLOR_DEC_ROW0=%s\n", gb.size(),
           join(two.row(0)).c_str(), gd.empty() ? "empty" : join(gd.row(0)).c_str());
  }
  {
    std::vector<uchar> gb;
    cv::imencode(".gif", g3, gb);
    cv::Mat gd = cv::imdecode(gb, cv::IMREAD_UNCHANGED);
    shape("GIF8UC3", gd);
    printf("GIF8UC3_FNV=%016" PRIx64 "\n",
           fnv1a64(std::string((const char*)gd.data, gd.total() * gd.elemSize())));
  }
  {
    std::vector<uchar> jbuf;
    cv::imencode(".jpg", g3, jbuf, {cv::IMWRITE_JPEG_QUALITY, 95});
    cv::Mat j = cv::imdecode(jbuf, cv::IMREAD_COLOR);
    shape("JPG", j);
    printf("JPG_MAXDIFF=%d\n", j.empty() ? -1 : (int)cv::norm(j, g3, cv::NORM_INF));
  }

  // ---- core: persistence (XML / YAML / JSON writer + parser) --------------------------------
  filestorage("YAML", cv::FileStorage::FORMAT_YAML);
  filestorage("XML", cv::FileStorage::FORMAT_XML);
  filestorage("JSON", cv::FileStorage::FORMAT_JSON);
  {
    cv::FileStorage b;
    b.open(".mem", cv::FileStorage::WRITE_BASE64 | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_YAML);
    b << "m" << g16;
    std::string txt = b.releaseAndGetString();
    printf("FS_B64_FNV=%016" PRIx64 "\n", fnv1a64(txt));
    cv::FileStorage r(txt, cv::FileStorage::READ | cv::FileStorage::MEMORY);
    cv::Mat m;
    r["m"] >> m;
    printf("FS_B64_RT=%s\n", same(m, g16) ? "OK" : "DIFF");
  }

  // ---- core: matrix algebra (the core_fuzzer surface) ----------------------------------------
  cv::Mat A = cv::Mat_<double>({3, 3}, {4, 7, 2, 3, 6, 1, 2, 5, 3});
  printf("DET=%d\n", cvRound(cv::determinant(A)));
  cv::Mat Ai = A.inv();
  printf("INV9=%s\n", join(Ai, 9.0).c_str());
  printf("AAI=%s\n", join(A * Ai).c_str());
  printf("T=%s\n", join(A.t()).c_str());
  printf("DIAG=%s\n", join(A.diag()).c_str());
  printf("TRACE=%d\n", cvRound(cv::trace(A)[0]));
  {
    std::vector<cv::Mat> ch;
    cv::split(g3, ch);
    cv::Mat merged;
    cv::merge(ch, merged);
    printf("SPLIT_SUMS=%.0f,%.0f,%.0f\n", cv::sum(ch[0])[0], cv::sum(ch[1])[0], cv::sum(ch[2])[0]);
    printf("MERGE_RT=%s\n", same(merged, g3) ? "OK" : "DIFF");
    cv::Mat tg = g3.t();
    shape("G3T", tg);
    printf("G3T_PIX_1_2=%s\n", join(tg(cv::Rect(2, 1, 1, 1))).c_str());
  }

  // ---- imgproc: text rendering (HarfBuzz shaping + hb-raster) --------------------------------
  {
    cv::Mat canvas = cv::Mat::zeros(64, 256, CV_8UC1);
    cv::putText(canvas, "Mayhem", cv::Point(8, 44), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255), 2,
                cv::LINE_AA);
    printf("TEXT_NZ=%d\n", cv::countNonZero(canvas));
    sum4("TEXT", canvas);
    int base = 0;
    cv::Size ts = cv::getTextSize("Mayhem", cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &base);
    printf("TEXT_SIZE=%dx%d+%d\n", ts.width, ts.height, base);
    cv::Mat c2 = cv::Mat::zeros(64, 256, CV_8UC1);
    cv::FontFace italic("italic");
    cv::putText(c2, "OpenCV 5.x", cv::Point(8, 44), cv::Scalar(255), italic, 24, 400);
    printf("TEXT2_NZ=%d\n", cv::countNonZero(c2));
    sum4("TEXT2", c2);
  }

  // ---- imgproc: color conversion + resampling ------------------------------------------------
  {
    cv::Mat bgr(1, 3, CV_8UC3);
    bgr.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 0, 0);
    bgr.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 255, 0);
    bgr.at<cv::Vec3b>(0, 2) = cv::Vec3b(0, 0, 255);
    cv::Mat g, hsv, lab;
    cv::cvtColor(bgr, g, cv::COLOR_BGR2GRAY);
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
    printf("GRAY=%s\nHSV=%s\nLAB=%s\n", join(g).c_str(), join(hsv).c_str(), join(lab).c_str());
    cv::Mat q(4, 4, CV_8UC1);
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) q.at<uchar>(r, c) = (uchar)(r * 4 + c);
    cv::Mat nn, area, lin;
    cv::resize(q, nn, cv::Size(2, 2), 0, 0, cv::INTER_NEAREST);
    cv::resize(q, area, cv::Size(2, 2), 0, 0, cv::INTER_AREA);
    cv::resize(q, lin, cv::Size(8, 8), 0, 0, cv::INTER_LINEAR);
    printf("RESIZE_NN=%s\nRESIZE_AREA=%s\nRESIZE_LIN_SUM=%.0f\n", join(nn).c_str(), join(area).c_str(),
           cv::sum(lin)[0]);
    cv::Mat blur;
    cv::GaussianBlur(g3, blur, cv::Size(3, 3), 0);
    sum4("BLUR", blur);
    cv::Mat th;
    cv::threshold(g1, th, 127, 255, cv::THRESH_BINARY);
    printf("THRESH_NZ=%d\n", cv::countNonZero(th));
  }

  printf("KAT_END=1\n");
  return 0;
}
