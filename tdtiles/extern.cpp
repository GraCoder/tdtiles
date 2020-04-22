#include "stdafx.h"
#include "extern.h"
#include "boost/filesystem.hpp"



bool mkdirs(const char* path)
{
    return boost::filesystem::create_directories(path);
}

bool write_file(const char* filename, const char* buf, unsigned long buf_len)
{
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

void log_error(const char* msg)
{
}

