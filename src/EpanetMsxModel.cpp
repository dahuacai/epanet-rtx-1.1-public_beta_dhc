//
//  EpanetMsxModel.cpp
//  epanet-rtx
//
//  Open Water Analytics [wateranalytics.org]
//  See README.md and license.txt for more information
//


#include "EpanetMsxModel.h"

using namespace RTX;
using namespace std;

EpanetMsxModel::EpanetMsxModel() {
  
}

EpanetMsxModel::~EpanetMsxModel() {
  
}

void EpanetMsxModel::loadMsxFile(std::string file) {
  
  _msxFile = file;
  MSXopen((char*)file.c_str());
  
}


void EpanetMsxModel::solveSimulation(time_t time) {
  EpanetModel::solveSimulation(time);
  
  
  
}

void EpanetMsxModel::stepSimulation(time_t time) {
  EpanetModel::stepSimulation(time);
  
  long t, tleft;
  MSXstep(&t, &tleft);
  
  
}

