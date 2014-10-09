#include <iostream>
#include "ConfigProject.h"
#include "SqlitePointRecord.h"


using namespace std;
using namespace RTX;

int main(int argc, const char * argv[])
{
  
  
  
  
  
  
  
  
  
  
  
  time_t someTime = 1353330000; // unix-time 2012-11-19 8:00:00 EST
  long duration = 60 * 60 * 24 * 12; // 12 days
  
  string forwardSimulationConfig("");
  if (argc > 1) {
    forwardSimulationConfig = string( argv[1] );
  }
  
  
  
  
  ConfigProject config;
  Model::sharedPointer model;
  
  try {
    config.loadProjectFile(forwardSimulationConfig);
    
    model = config.model();
    
    // reset tank levels (forget what the model says)
    BOOST_FOREACH(Tank::sharedPointer t, model->tanks()) {
      t->setResetLevelNextTime(true);
    }
    
    model->setInitialQuality(350.);
    
    cout << "RTX: Running simulation for..." << endl;
    cout << *model;
    
    PointRecord::sharedPointer record = config.defaultRecord();
    
    model->runExtendedPeriod(someTime, someTime + duration);
    
    cout << "... done" << endl;
    
  } catch (string err) {
    cerr << err << endl;
  }
  
}

