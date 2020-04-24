// tdtiles.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "stdafx.h"
#include <iostream>
#include "shp.h"
#include "osgb.h"
#include "cmdline.h"




int main(int argc, char **argv)
{
    using namespace std;

    cmdline::parser p;
    p.add<string>("format", 'f', "input format");
    p.add<string>("input", 'i', "input filename");
    p.add<string>("output", 'o', "output dir");
    p.add<string>("height", 'h', "shape height field", false);
    p.add<string>("lotitude", 'x', "lotitude of the bounding box", false);
    p.add<string>("latitude", 'y', "latitude of the bounding box", false);
    p.add<string>("xoffset", 0, "east offset at the geo position (meters) todo", false);
    p.add<string>("yoffset", 0, "north offset at the geo position (meters) todo", false);
    p.add<string>("zoffset", 0, "vertical offset at the geo position", false);

    p.parse_check(argc, argv);

    auto format = p.get<string>("format");
    auto input = p.get<string>("input");
    auto output = p.get<string>("output");

    if (p.exist("lotitude") && p.exist("latitude")) {
      config.lo = p.get<double>("lotitude");
      config.la = p.get<double>("latitude");
      if (config.lo < -180 || config.lo > 180 || config.la < -90 || config.la > 90) {
        std::cerr << "logtitude and latitude must be valid in wgs84.";
        return 1;
      }
    }
    if (p.exist("xoffset")) config.xoff = p.get<double>("xoffset");
    if (p.exist("yoffset")) config.yoff = p.get<double>("yoffset");
    if (p.exist("zoffset")) config.zoff = p.get<double>("zoffset");

    bool ret = true;
    if (format == "shape") {
        auto hfield = p.get<string>("height");
        if (hfield.empty()) {
            std::cerr << "shape need --height to specific the height field to excude the face geometry.\n" << p.usage();
            return 1;
        }
        ret = shape_batch_convert(input.c_str(), 0, output.c_str(), hfield.c_str());
    }else if (format == "osgb"){
        ret = osgb_batch_convert(input.c_str(), output.c_str());
    } else {
        std::cerr << "unknown format" << std::endl;
        return 1;
    }

    return !ret;
}

