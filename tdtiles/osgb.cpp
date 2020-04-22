#include "stdafx.h"
#include "osgb.h"
#include "boost/filesystem.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/xml_parser.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"


extern "C" {
    bool epsg_convert(int insrs, double* val, const char* path);
    bool wkt_convert(const char* wkt, double* val, const char* path);
    const char* osgb23dtile_path(const char* in_path, const char* out_path, double* box, int* len, double x, double y, int max_lvl);
}

struct TaskInfo {
    std::string osgbPath;
    std::string b3dmPath;
};

std::vector<TaskInfo> walk_osgb_dir(const std::string &dir,
                                    const std::string &out) {
  using namespace std;
  std::vector<TaskInfo> ret;
  boost::filesystem::path desPath(dir);
  boost::filesystem::recursive_directory_iterator iterb(desPath), itere;
  std::set<std::string> pathTmp;
  while (iterb != itere) {
    auto pa = iterb->path().string();
    if (boost::filesystem::is_regular_file(pa) &&
        boost::filesystem::extension(pa) == ".osgb") {
      auto desPath = out + pa.substr(dir.size());
      auto desDir = boost::filesystem::path(desPath).parent_path().string();
      pathTmp.insert(desDir);
      ret.push_back({pa, desDir});
    }
    iterb++;
  }
  for(auto iter = pathTmp.begin(); iter != pathTmp.end(); iter++)
    boost::filesystem::create_directories(*iter);
  return ret;
}

bool osgb_batch_convert_core(const std::string &in, const std::string &out,
                             double cx, double cy, double cz = 0,
                             int level = INT_MAX, double zRotate = 0) {
  auto dataPath = in + "/Data";
  if (!boost::filesystem::exists(dataPath)) {
    std::cerr << dataPath << " is not exist" << std::endl;
    return false;
  }
  auto outDataPath = out + "/Data";
  std::vector<TaskInfo> tasks = walk_osgb_dir(dataPath, outDataPath);
  double rx = osg::DegreesToRadians(cx), ry = osg::DegreesToRadians(cy);
  double box[6]; int jsRetLen;
  for (auto& task : tasks) {
    const char *ptr = osgb23dtile_path(task.osgbPath.c_str(), task.b3dmPath.c_str(), box, &jsRetLen, rx, ry, level);
    if (ptr == nullptr) {
        std::cerr << task.osgbPath << "convert failed." << std::endl;
        continue;
    }
    std::string jsonRet(ptr, jsRetLen);
  }
  return false;
}

bool osgb_batch_convert(const std::string &in, const std::string &out) {
  using namespace boost::filesystem;
  using namespace boost::property_tree;
  if (!is_directory(in) || !exists(in + "/metadata.xml")) {
    std::cerr << "path" + in +
                     "must be a directory and has a file named metadata.xml"
              << std::endl;
    return false;
  }
  ptree pt, root;
  try {
    read_xml(in + "/metadata.xml", pt);
    root = pt.get_child("ModelMetadata");
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    return false;
  }
  std::string srs, srsOrigin;
  for (auto iter = root.begin(); iter != root.end(); iter++) {
    if (iter->first == "SRS")
      srs = iter->second.data();
    else if (iter->first == "SRSOrigin")
      srsOrigin = iter->second.data();
  }
  double centerx, centery;
  std::vector<std::string> outvec;
  boost::algorithm::split(outvec, srs, boost::is_any_of(":"));
  if (outvec.size() > 1) {
    if (outvec[0] == "ENU") {
      boost::algorithm::split(outvec, outvec[1], boost::is_any_of(","));
      if (outvec.size() > 1) {
        centerx = boost::lexical_cast<double>(outvec[1]);
        centery = boost::lexical_cast<double>(outvec[0]);
      } else
        std::cerr << "parse ENU points error";
    } else if (outvec[0] == "EPSG") {
      int epsgId = boost::lexical_cast<int>(outvec[1]);
      std::vector<std::string> posVec;
      boost::algorithm::split(posVec, srsOrigin, boost::is_any_of(","));
      if (posVec.size() < 2) {
        std::cerr << "epsg point not enough";
        return false;
      }
      std::vector<double> dvec(posVec.size());
      for (int i = 0; i < posVec.size(); i++)
        dvec[i] = boost::lexical_cast<double>(posVec[i]);
      auto gddata = initial_path().string() + "gdal_data";
      putenv((std::string("gdal_data=") + gddata).c_str());
      if (epsg_convert(epsgId, &dvec[0], gddata.c_str())) {
        centerx = dvec[0];
        centery = dvec[1];
      } else {
        std::cerr << "epsg convert failed.";
        return false;
      }
    } else {
      std::cerr << "EPSG or ENU is expected in SRS";
    }
  } else {
    std::vector<std::string> posVec;
    boost::algorithm::split(posVec, srsOrigin, boost::is_any_of(","));
    if (posVec.size() < 2) {
      std::cerr << "wtk point not enough";
      return false;
    }
    std::vector<double> dvec(posVec.size());
    for (int i = 0; i < posVec.size(); i++)
      dvec[i] = boost::lexical_cast<double>(posVec[i]);
    auto gddata = initial_path().string() + "gdal_data";
    putenv((std::string("gdal_data=") + gddata).c_str());
    if (wkt_convert(srs.c_str(), &dvec[0], gddata.c_str())) {
      centerx = dvec[0];
      centery = dvec[1];
    } else {
      std::cerr << "wkt convert failed.";
      return false;
    }
  }
  return osgb_batch_convert_core(in, out, centerx, centery);
}
