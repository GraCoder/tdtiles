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
    p.add("yup", 0, "y axis up");

    p.parse_check(argc, argv);

    auto format = p.get<string>("format");
    auto input = p.get<string>("input");
    auto output = p.get<string>("output");
    bool yup = p.exist("yup");

    bool ret = true;
    if (format == "shape") {
        auto hfield = p.get<string>("height");
        if (hfield.empty()) {
            std::cerr << "shape need --height\n" << p.usage();
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

