#include "stdafx.h"
#include "extern.h"
#include "boost/filesystem.hpp"

Config config = {std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::quiet_NaN(), 0, 0, 0};

bool mkdirs(const char *path) {
  return boost::filesystem::create_directories(path);
}

bool write_file(const std::string &f, std::string &c) {
  return write_file(f.c_str(), c.c_str(), c.size());
}

bool write_file(const char *filename, const char *buf, unsigned long buf_len) {
  FILE *fp = fopen(filename, "wb+");
  if (!fp)
    return false;
  size_t sz = fwrite(buf, 1, buf_len, fp);
  bool ret = true;
  if (sz != buf_len)
    ret = false;
  fclose(fp);
  return ret;
}

void log_error(const char *msg) {}
