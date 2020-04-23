#include "stdafx.h"
#include "osgb.h"
#include "json.hpp"
#include "boost/filesystem.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/xml_parser.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"


extern "C" {
bool epsg_convert(int insrs, double *val, const char *path);
bool wkt_convert(const char *wkt, double *val, const char *path);
void *osgb23dtile_path(const char *in_path, const char *out_path, double *box,
                       int *len, double x, double y, int max_lvl);
void transform_c(double center_x, double center_y, double height_min,
                 double *ptr);
}

struct TaskInfo {
  std::string osgbPath;
  std::string b3dmPath;
};

struct ResultTile {
  std::string b3dmPath;
  std::string json;
  double bbox[6];
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
    if (boost::filesystem::is_directory(pa)){
      auto fi = pa + '/' + iterb->path().stem().string() + ".osgb";
      if (boost::filesystem::exists(fi)) {
        auto xx = out + boost::filesystem::path(fi).parent_path().string().substr(dir.size());
        ret.push_back( {fi, xx });
        pathTmp.insert(xx);
      }
    }
    iterb++;
  }
  for (auto iter = pathTmp.begin(); iter != pathTmp.end(); iter++)
    boost::filesystem::create_directories(*iter);
  return ret;
}

void box_to_tileset_box(double box[6], double oBox[12]) {
  oBox[0] = (box[0] + box[3]) / 2.0;
  oBox[1] = (box[1] + box[4]) / 2.0;
  oBox[2] = (box[2] + box[5]) / 2.0;
  oBox[3] = (box[0] - box[3]) / 2.0;
  oBox[4] = 0;
  oBox[5] = 0;
  oBox[6] = 0;
  oBox[7] = (box[1] - box[4]) / 2.0;
  oBox[8] = 0;
  oBox[9] = 0;
  oBox[10] = 0;
  oBox[11] = (box[2] - box[5]) / 2.0;
}

double cal_geometric_error(double cy, int lvl) {
  const double pi = 3.1415926;
  double x = cy * pi / 180.0;
  double r = cos(x) * 2.0 * pi * 6378137.0;
  return 4.9 * r / (256 * pow(2, lvl - 2));
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
  double box[6];
  int jsRetLen;
  std::vector<ResultTile> tileVec;
  for (auto &task : tasks) {
    void *ptr = osgb23dtile_path(task.osgbPath.c_str(), task.b3dmPath.c_str(),
                                 box, &jsRetLen, rx, ry, level);
    if (ptr == nullptr) {
      std::cerr << task.osgbPath << "convert failed." << std::endl;
      continue;
    }
    std::string jsonRet((char *)ptr, jsRetLen);
    free(ptr);
    ResultTile tileTmp{std::move(task.b3dmPath), std::move(jsonRet), *box};
    memcpy(tileTmp.bbox, box, sizeof(double) * 6);
    tileVec.push_back(tileTmp);
  }
  double rootBox[6] = {
      -DBL_MAX, -DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX,
  };
  for (auto &tile : tileVec) {
    for (int i = 0; i < 3; i++) {
      if (tile.bbox[i] > rootBox[i])
        rootBox[i] = tile.bbox[i];
    }
    for (int i = 3; i < 6; i++) {
      if (tile.bbox[i] < rootBox[i])
        rootBox[i] = tile.bbox[i];
    }
  }
  double transform[16];
  transform_c(cx, cy, cz, transform);
  double bbox[12];
  box_to_tileset_box(rootBox, bbox);
  std::string rootRJson = R"(
        {
            "asset": {
                "version": "1.0",
                "gltfUpAxis": "Y"
            },
            "geometricError": 2000,
            "root" : {
                "transform" : [],
                "boundingVolume" : {
                    "box": []
                },
                "geometricError": 2000,
                "children": []
            }
        }
  )";
  std::string childRJson = R"(
        {
            "boundingVolume": {
                "box": []
            },
            "geometricError": 1000,
            "content": {
                "uri" : ""
            }
        }
  )";
  std::string tileRJson = R"(
        {
            "asset": {
                "version": "1.0",
                "gltfUpAxis":"Y"
            }
        }
  )";
  auto rootJson = nlohmann::json::parse(rootRJson);
  auto childJson = nlohmann::json::parse(childRJson);
  auto tileJson = nlohmann::json::parse(tileRJson);

  rootJson["root"]["transform"] = transform;
  rootJson["root"]["boundingVolume"]["box"] = bbox;

  std::vector<nlohmann::json> children;
  for (auto &tile : tileVec) {
    auto chJson = nlohmann::json::parse(tile.json);
    auto bboxJson = chJson["boundingVolume"]["box"]; 

    childJson["boundingVolume"]["box"] = bboxJson;
    auto url = (tile.b3dmPath + "/tileset.json").substr(out.size());
    std::replace(url.begin(), url.end(), '\\', '/');

    childJson["content"]["uri"] = "." + url;
    children.push_back(childJson);

    tileJson["root"] = chJson;
    write_file(tile.b3dmPath + "/tileset.json", tileJson.dump(4));
  }

  rootJson["root"]["children"] = children;
  write_file(out + "/tileset.json", rootJson.dump(4));
  return true;
}

bool osgb_batch_convert(const std::string &in, const std::string &out) {
  using namespace boost::filesystem;
  using namespace boost::property_tree;
  if (!is_directory(in) || !exists(in + "/metadata.xml")) {
    std::cerr << "path \"" + in +
                     "\" must be a directory and has a file named metadata.xml"
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
  double centerx = 0, centery = 0;
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
