/**
 * @File Name: idm_edadb.cpp
 * @Brief :
 * @Author : zhiyi wang
 * @Version : 1.0
 */

#include "idm_edadb.h"


namespace idm {

bool DataManagerEdadb::readDef(string path) {
  std::cout << "============================================" << std::endl;
  std::cout << "[iDM] read DEF using EDADB backend: " << path << std::endl;
  std::cout << "============================================" << std::endl;
  return true;
}



bool DataManagerEdadb::saveDef(string path) {
  std::cout << "============================================" << std::endl;
  std::cout << "[iDM] save DEF using EDADB backend: " << path << std::endl;
  std::cout << "============================================" << std::endl;
  return true;
}

} // namespace idm