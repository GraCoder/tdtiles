#include "stdafx.h"
#include "shp.h"
#include "json.hpp"
#include "boost/filesystem.hpp"

extern "C" {
extern bool shp23dtile(const char *filename, int layer_id, const char *dest,
                       const char *height);
}

std::vector<std::string> walk_shp_dir(const char *dir) {
  using namespace std;
  vector<string> ret;
  boost::filesystem::path desPath(dir);
  boost::filesystem::recursive_directory_iterator iterb(desPath), itere;
  while (iterb != itere) {
    auto pa = iterb->path().string();
    if (boost::filesystem::is_regular_file(pa) &&
        boost::filesystem::extension(pa) == ".json" &&
        boost::filesystem::basename(pa) != "tileset")
      ret.push_back(pa);
    iterb++;
  }
  return ret;
}

bool shape_batch_convert(const char *filename, int layer_id, const char *dest,
                         const char *height) {
  bool ret = true;
  ret = shp23dtile(filename, layer_id, dest, height);
  if (!ret)
    return ret;

  nlohmann::json js;
  js["asset"]["version"] = "0.0";
  js["asset"]["gltfUpAxis"] = "Y";
  js["geometricError"] = 200;
  nlohmann::json root;
  root["refine"] = "REPLACE";
  root["geometricError"] = 200;

  auto tiles = walk_shp_dir(dest);
  float region[6] = {FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MIN};
  int minArray[3] = {0, 1, 4}, maxArray[3] = {2, 3, 5};
  std::vector<nlohmann::json> childField;
  for (auto iterb = tiles.begin(); iterb != tiles.end(); iterb++) {
    std::ifstream fs(*iterb);
    auto jsTile = nlohmann::json::parse(fs);
    fs.close();
    if (jsTile.is_null())
      continue;
    auto &regTmp = jsTile["root"]["boundingVolume"]["region"];
    if (regTmp.is_array()) {
      for (auto idx : minArray) {
        if (regTmp[idx] < region[idx])
          region[idx] = regTmp[idx];
      }
      for (auto idx : maxArray) {
        if (regTmp[idx] > region[idx])
          region[idx] = regTmp[idx];
      }
    }
    childField.push_back(jsTile["root"]);
  }

  root["boundingVolume"]["region"] = region;
  root["children"] = childField;
  js["root"] = root;

  std::ofstream tileSetfs(std::string(dest) + "/tileset.json");
  tileSetfs << js.dump(4);
  tileSetfs.close();
  return ret;
}
