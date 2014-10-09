#include "SqliteProjectFile.h"

#ifndef RTX_NO_ODBC
#include "OdbcDirectPointRecord.h"
#endif
#ifndef RTX_NO_MYSQL
#include "MysqlPointRecord.h"
#endif
#include "SqlitePointRecord.h"

#include "TimeSeries.h"
#include "ConstantTimeSeries.h"
#include "SineTimeSeries.h"
#include "Resampler.h"
#include "MovingAverage.h"
#include "FirstDerivative.h"
#include "AggregatorTimeSeries.h"
#include "FirstDerivative.h"
#include "OffsetTimeSeries.h"
#include "CurveFunction.h"
#include "ThresholdTimeSeries.h"
#include "ValidRangeTimeSeries.h"
#include "GainTimeSeries.h"
#include "MultiplierTimeSeries.h"
#include "InversionTimeSeries.h"
#include "FailoverTimeSeries.h"
#include "TimeOffsetTimeSeries.h"
#include "WarpingTimeSeries.h"
#include "StatsTimeSeries.h"
#include "OutlierExclusionTimeSeries.h"

#include "EpanetModel.h"

#include <boost/range/adaptors.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <fstream>

#define typeEquals(x) RTX_STRINGS_ARE_EQUAL(type,x)


typedef const unsigned char* sqltext;

using namespace RTX;
using namespace std;




/////////////////////////////////////////////////////////////
static int sqlRequiredUserVersion = 10;

//---------------------------------------------------------//
static string sqlSelectRecords =    "select id,name,type from point_records";
static string sqlSelectClocks = "select id,name,type,period,offset from clocks";
static string sqlSelectTimeseries = "select id,type from time_series";
static string sqlGetTsById =        "select name,units,record,clock from time_series where id=?";
//static string sqlGetTsModularById = "select * from time_series_modular left join time_series_extended using (id) where id=?";
//static string sqlGetTsExtendedById = "select type,value_1,value_2,value_3 from time_series left join time_series_extended using (id) where id=?";
static string sqlGetTsPropertiesById = "select key,value from time_series_properties where ref_table is null and id=?";
static string sqlGetTsSourceById = "select key,value from time_series_properties where ref_table=\"time_series\" and id=?";
static string sqlGetTsClockParameterById = "select key,value from time_series_properties where ref_table=\"clocks\" and id=?";
static string sqlGetAggregatorSourcesById = "select time_series,multiplier from time_series_aggregator where aggregator_id=?";
static string sqlGetCurveCoordinatesByTsId = "select x,y from ( select curve,name,curve_id,time_series_curves.id as tsId from time_series_curves left join curves on (curve = curve_id) ) left join curve_data using (curve_id) where tsId=?";
static string sqlGetModelElementParams = "select time_series, model_element, parameter from element_parameters";
static string sqlGetModelNameAndTypeByUid = "select name,type from model_elements where uid=?";

static string selectModelFileStr = "select value from meta where key = \"model_contents\"";

//---------------------------------------------------------//
static string dbTimeseriesName =  "TimeSeries";
static string dbConstantName =   "Constant";
static string dbSineName = "Sine";
static string dbResamplerName =  "Resampler";
static string dbMovingaverageName = "MovingAverage";
static string dbAggregatorName = "Aggregator";
static string dbDerivativeName = "FirstDerivative";
static string dbOffsetName =     "Offset";
static string dbCurveName =      "Curve";
static string dbThresholdName =  "Threshold";
static string dbValidrangeName = "ValidRange";
static string dbGainName =       "Gain";
static string dbMultiplierName = "Multiplier";
static string dbInversionName = "Inversion";
static string dbFailoverName = "Failover";
static string dbLagName = "Lag";
static string dbWarpName = "Warp";
static string dbStatsName = "Stats";
static string dbOutlierName = "OutlierExclusion";
//---------------------------------------------------------//
static string dbOdbcRecordName =  "odbc";
static string dbMysqlRecordName = "mysql";
static string dbSqliteRecordName = "sqlite";
//---------------------------------------------------------//
/////////////////////////////////////////////////////////////

static map<int,int> mapping() {
  map<int,int> m;
  m[1] = 1;
  return m;
}

// local convenience classes
typedef struct {
  TimeSeries::sharedPointer ts;
  int uid;
  string type;
} tsListEntry;

typedef struct {
  PointRecord::sharedPointer record;
  int uid;
  string type;
} pointRecordEntity;

typedef struct {
  int tsUid;
  int modelUid;
  string param;
} modelInputEntry;

enum SqliteModelParameterType {
  ParameterTypeJunction  = 0,
  ParameterTypeTank      = 1,
  ParameterTypeReservoir = 2,
  ParameterTypePipe      = 3,
  ParameterTypePump      = 4,
  ParameterTypeValve     = 5
};


// optional compile-in databases
namespace RTX {
  class PointRecordFactory {
  public:
    static PointRecord::sharedPointer createSqliteRecordFromRow(sqlite3_stmt *stmt);
    static PointRecord::sharedPointer createCsvPointRecordFromRow(sqlite3_stmt *stmt);
#ifndef RTX_NO_ODBC
    static PointRecord::sharedPointer createOdbcRecordFromRow(sqlite3_stmt *stmt);
#endif
#ifndef RTX_NO_MYSQL
    static PointRecord::sharedPointer createMysqlRecordFromRow(sqlite3_stmt *stmt);
#endif
  };
}



void SqliteProjectFile::loadProjectFile(const string& path) {
  _path = path;
  char *zErrMsg = 0;
  int returnCode;
  returnCode = sqlite3_open_v2(_path.c_str(), &_dbHandle, SQLITE_OPEN_READONLY, NULL);
  if( returnCode ){
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(_dbHandle));
    sqlite3_close(_dbHandle);
    return;
  }
  
  
  // check schema version
  int databaseVersion = -1;
  sqlite3_stmt *stmt_version;
  if(sqlite3_prepare_v2(_dbHandle, "PRAGMA user_version;", -1, &stmt_version, NULL) == SQLITE_OK) {
    while(sqlite3_step(stmt_version) == SQLITE_ROW) {
      databaseVersion = sqlite3_column_int(stmt_version, 0);
    }
  }
  
  if (databaseVersion < sqlRequiredUserVersion) {
    cerr << "Config Database Schema version not compatible. Require version " << sqlRequiredUserVersion << " or greater." << endl;
    return;
  }
  
  
  
  // load everything
  loadRecordsFromDb();
  loadClocksFromDb();
  loadTimeseriesFromDb();
  loadModelFromDb();
  
  setModelInputParameters();
  
  
  // close db
  sqlite3_close(_dbHandle);
}

void SqliteProjectFile::saveProjectFile(const string& path) {
  // nope
}

void SqliteProjectFile::clear() {
  
}


PointRecord::sharedPointer SqliteProjectFile::defaultRecord() {
  
}

Model::sharedPointer SqliteProjectFile::model() {
  return _model;
}

RTX_LIST<TimeSeries::sharedPointer> SqliteProjectFile::timeSeries() {
  RTX_LIST<TimeSeries::sharedPointer> list;
  BOOST_FOREACH(TimeSeries::sharedPointer ts, _timeseries | boost::adaptors::map_values) {
    list.push_back(ts);
  }
  return list;
}

RTX_LIST<Clock::sharedPointer> SqliteProjectFile::clocks() {
  RTX_LIST<Clock::sharedPointer> list;
  BOOST_FOREACH(Clock::sharedPointer c, _clocks | boost::adaptors::map_values) {
    list.push_back(c);
  }
  return list;
}

RTX_LIST<PointRecord::sharedPointer> SqliteProjectFile::records() {
  RTX_LIST<PointRecord::sharedPointer> list;
  BOOST_FOREACH(PointRecord::sharedPointer pr, _records | boost::adaptors::map_values) {
    list.push_back(pr);
  }
  return list;
}




#pragma mark - private methods

void SqliteProjectFile::loadRecordsFromDb() {
  
  RTX_LIST<pointRecordEntity> recordEntities;
  
  // set up the function pointers for creating the new records.
  typedef PointRecord::sharedPointer (*PointRecordFp)(sqlite3_stmt*);
  map<string,PointRecordFp> prCreators;
  
#ifndef RTX_NO_ODBC
  prCreators[dbOdbcRecordName] = PointRecordFactory::createOdbcRecordFromRow;
#endif
#ifndef RTX_NO_MYSQL
  prCreators[dbMysqlRecordName] = PointRecordFactory::createMysqlRecordFromRow;
#endif
  
  prCreators[dbSqliteRecordName] = PointRecordFactory::createSqliteRecordFromRow;
  
  
  sqlite3_stmt *stmt;
  
  int retCode = sqlite3_prepare_v2(_dbHandle, sqlSelectRecords.c_str(), -1, &stmt, NULL);
  if (retCode != SQLITE_OK) {
    cerr << "can't prepare statement: " << sqlSelectRecords << " -- error: " << sqlite3_errmsg(_dbHandle) << endl;
    return;
  }
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    // we have a row ready
    int uid = sqlite3_column_int(stmt, 0);
    string name = string((char*)sqlite3_column_text(stmt, 1));
    string type = string((char*)sqlite3_column_text(stmt, 2));
    
    cout << "record: " << uid << " \"" << name << "\" (" << type << ")" << endl;
    
    // find the right function to create this type of record
    PointRecordFp creator = prCreators[type];
    PointRecord::sharedPointer record;
    if (creator) {
      record = creator(stmt);
    }
    
    if (record) {
      record->setName(name);
      _records[uid] = record;
      pointRecordEntity entity;
      entity.record = record;
      entity.type = type;
      entity.uid = uid;
      recordEntities.push_back(entity);
    }
    else {
      cerr << "could not create point record. type \"" << type << "\" not supported." << endl;
    }
    
  }
  
  sqlite3_finalize(stmt);
  
  
  
  
  // now load up each point record's connection attributes.
  BOOST_FOREACH(const pointRecordEntity& entity, recordEntities) {
    
    if (RTX_STRINGS_ARE_EQUAL(entity.type, "sqlite")) {
      string sqlGetSqliteAttr = "select dbPath from point_records where id=?";
      sqlite3_prepare_v2(_dbHandle, sqlGetSqliteAttr.c_str(), -1, &stmt, NULL);
      sqlite3_bind_int(stmt, 1, entity.uid);
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        string dbPath = string((char*)sqlite3_column_text(stmt, 0));
        boost::dynamic_pointer_cast<SqlitePointRecord>(entity.record)->setPath(dbPath);
      }
      sqlite3_reset(stmt);
      sqlite3_finalize(stmt);
    }
    
  }
  
  
  
  
  
}

void SqliteProjectFile::loadClocksFromDb() {
  
  sqlite3_stmt *stmt;
  int retCode = sqlite3_prepare_v2(_dbHandle, sqlSelectClocks.c_str(), -1, &stmt, NULL);
  if (retCode != SQLITE_OK) {
    cerr << "can't prepare statement: " << sqlSelectClocks << " -- error: " << sqlite3_errmsg(_dbHandle) << endl;
    return;
  }
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    //static string sqlSelectClocks = "select id,name,type,period,offset from clocks";
    int uid = sqlite3_column_int(stmt, 0);
    string name = string((char*)sqlite3_column_text(stmt, 1));
    string type = string((char*)sqlite3_column_text(stmt, 2));
    int period = sqlite3_column_int(stmt, 3);
    int offset = sqlite3_column_int(stmt, 4);
    
    if (RTX_STRINGS_ARE_EQUAL(type, "Regular")) {
      Clock::sharedPointer clock( new Clock(period,offset) );
      clock->setName(name);
      _clocks[uid] = clock;
    }
    
  }
  
}

void SqliteProjectFile::loadTimeseriesFromDb() {
  
  RTX_LIST<tsListEntry> tsList;
  sqlite3_stmt *stmt;
  
  int retCode = sqlite3_prepare_v2(_dbHandle, sqlSelectTimeseries.c_str(), -1, &stmt, NULL);
  if (retCode != SQLITE_OK) {
    cerr << "can't prepare statement: " << sqlSelectTimeseries << " -- error: " << sqlite3_errmsg(_dbHandle) << endl;
    return;
  }
  
  // CREATE ALL THE TIME SERIES
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    // we have a row ready
    int uid = sqlite3_column_int(stmt, 0);
    sqltext typeChar = sqlite3_column_text(stmt, 1);
    string type((char*)typeChar);
    TimeSeries::sharedPointer ts = newTimeseriesWithType(type);
    if (ts) {
      tsListEntry entry;
      entry.ts = ts;
      entry.uid = uid;
      entry.type = type;
      tsList.push_back(entry);
    }
  }
  sqlite3_finalize(stmt);
  
  // cache them, keyed by uid
  BOOST_FOREACH(tsListEntry entry, tsList) {
    _timeseries[entry.uid] = entry.ts;
  }
  
  // setting units, record, clock...
  BOOST_FOREACH(tsListEntry entry, tsList) {
    this->setBaseProperties(entry.ts, entry.uid);
  }
  
  
  
  // SET PROPERTIES
  retCode = sqlite3_prepare_v2(_dbHandle, sqlGetTsPropertiesById.c_str(), -1, &stmt, NULL);
  if (retCode != SQLITE_OK) {
    cerr << "can't prepare statement: " << sqlGetTsPropertiesById << " -- error: " << sqlite3_errmsg(_dbHandle) << endl;
    return;
  }
  BOOST_FOREACH(tsListEntry entry, tsList) {
    sqlite3_bind_int(stmt, 1, entry.uid);
    // get k-v pairs and use a setter function
    retCode = sqlite3_step(stmt);
    while (retCode == SQLITE_ROW) {
      string key((char*)sqlite3_column_text(stmt, 0));
      double val = sqlite3_column_double(stmt, 1);
      this->setPropertyValuesForTimeSeriesWithType(entry.ts, entry.type, key, val);
      retCode = sqlite3_step(stmt);
    }
    sqlite3_reset(stmt);
  }
  retCode = sqlite3_finalize(stmt);
  
  // SET SOURCES
  /*** ---> TODO ::
   need to figure out a smart way to ensure that downstream modules are affected by upstream setters.
   that is, make sure we're working from left to right.
   do we need a responder chain?
   
   generally we would expect all this to be ordered by id, with id increasing from "left to right"
   ***/
  
  // get time series sources and secondary sources, for modular types.
  retCode = sqlite3_prepare_v2(_dbHandle, sqlGetTsSourceById.c_str(), -1, &stmt, NULL);
  if (retCode != SQLITE_OK) {
    cerr << "can't prepare statement: " << sqlGetTsSourceById << " -- error: " << sqlite3_errmsg(_dbHandle) << endl;
    return;
  }
  
  BOOST_FOREACH(tsListEntry entry, tsList) {
    // if it doesn't take a source, then move on.
    ModularTimeSeries::sharedPointer mod = boost::dynamic_pointer_cast<ModularTimeSeries>(entry.ts);
    if (!mod) {
      continue;
    }
    
    // query for any upstream sources / secondary sources
    sqlite3_bind_int(stmt, 1, entry.uid);
    // for each row, find out what key it's setting and set it on this time series.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      string key = (char*)sqlite3_column_text(stmt, 0);
      int idx = sqlite3_column_int(stmt, 1);
      TimeSeries::sharedPointer upstreamTs = _timeseries[idx];
      if (!upstreamTs) {
        continue;
      }
      
      // possibilites for key: source, failover, multiplier, warp
      if (RTX_STRINGS_ARE_EQUAL(key, "source")) {
        mod->setSource(upstreamTs);
      }
      else if (RTX_STRINGS_ARE_EQUAL(key, "failover")) {
        boost::dynamic_pointer_cast<FailoverTimeSeries>(mod)->setFailoverTimeseries(upstreamTs);
      }
      else if (RTX_STRINGS_ARE_EQUAL(key, "multiplier")) {
        boost::dynamic_pointer_cast<MultiplierTimeSeries>(mod)->setMultiplier(upstreamTs);
      }
      else if (RTX_STRINGS_ARE_EQUAL(key, "warp")) {
        boost::dynamic_pointer_cast<WarpingTimeSeries>(mod)->setWarp(upstreamTs);
      }
      else {
        cerr << "unknown key: " << key << endl;
      }
    }
    sqlite3_reset(stmt); // get ready for next bind
  }
  retCode = sqlite3_finalize(stmt);
  
  
  
  // get parameterized clocks
  retCode = sqlite3_prepare_v2(_dbHandle, sqlGetTsClockParameterById.c_str(), -1, &stmt, NULL);
  if (retCode != SQLITE_OK) {
    cerr << "can't prepare statement: " << sqlGetTsClockParameterById << " -- error: " << sqlite3_errmsg(_dbHandle) << endl;
    return;
  }
  BOOST_FOREACH(tsListEntry entry, tsList) {
    // query for any upstream sources / secondary sources
    sqlite3_bind_int(stmt, 1, entry.uid);
    // if this returns a row, then this time series has a parameterized clock
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      string key = (char*)sqlite3_column_text(stmt, 0);
      int idx = sqlite3_column_int(stmt, 1);
      Clock::sharedPointer clock = _clocks[idx];
      if (!clock) {
        continue;
      }
      if (RTX_STRINGS_ARE_EQUAL(key, "samplingWindow")) {
        BaseStatsTimeSeries::sharedPointer bs = boost::dynamic_pointer_cast<BaseStatsTimeSeries>(entry.ts);
        bs->setWindow(clock);
      }
    }
    sqlite3_reset(stmt);
  }
  retCode = sqlite3_finalize(stmt);
  
  
  
  
  // ANY AGGREGATORS?
  sqlite3_prepare_v2(_dbHandle, sqlGetAggregatorSourcesById.c_str(), -1, &stmt, NULL);
  BOOST_FOREACH(tsListEntry entry, tsList) {
    if (RTX_STRINGS_ARE_EQUAL(entry.type, "Aggregator")) {
      AggregatorTimeSeries::sharedPointer agg = boost::dynamic_pointer_cast<AggregatorTimeSeries>(entry.ts);
      if (!agg) {
        cerr << "not an aggregator" << endl;
        continue;
      }
      // get sources.
      sqlite3_bind_int(stmt, 1, entry.uid);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        int idx = sqlite3_column_int(stmt, 0);
        double multiplier = sqlite3_column_double(stmt, 1);
        TimeSeries::sharedPointer upstream = _timeseries[idx];
        agg->addSource(upstream, multiplier);
      }
      sqlite3_reset(stmt);
    }
    
  }
  
  
  // ANY CURVE TIME SERIES?
  int curveId = 0;
  string curveName();
  
  BOOST_FOREACH(tsListEntry tsEntry, tsList) {
    if (RTX_STRINGS_ARE_EQUAL(tsEntry.type, "Curve")) {
      
      CurveFunction::sharedPointer curveTs = boost::dynamic_pointer_cast<CurveFunction>(tsEntry.ts);
      if (!curveTs) {
        cerr << "not an actual curve function object" << endl;
        continue;
      }
      
      // get the curve id
      sqlite3_stmt *getCurveIdStmt;
      sqlite3_prepare_v2(_dbHandle, sqlGetCurveCoordinatesByTsId.c_str(), -1, &stmt, NULL);
      sqlite3_bind_int(stmt, 1, tsEntry.uid);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        double x = sqlite3_column_double(stmt, 0);
        double y = sqlite3_column_double(stmt, 1);
        curveTs->addCurveCoordinate(x, y);
      }
    }
    
    
  }
  
  
  retCode = sqlite3_finalize(stmt);
  
  
}



void SqliteProjectFile::loadModelFromDb() {
  int ret;
  string modelContents;
  EpanetModel::sharedPointer enModel( new EpanetModel );
  // error here? implement the new version of this.
  // de-serialize the model file from meta.key="model_contents"
  sqlite3_stmt *stmt;
  ret = sqlite3_prepare_v2(_dbHandle, selectModelFileStr.c_str(), -1, &stmt, NULL);
  if (ret != SQLITE_OK) {
    cerr << "can't prepare statement: " << selectModelFileStr << " -- error: " << sqlite3_errmsg(_dbHandle) << endl;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    modelContents = string((char*)sqlite3_column_text(stmt, 0));
    try {
      boost::filesystem::path tempFile = boost::filesystem::temp_directory_path();
      boost::filesystem::path tempNameHash = boost::filesystem::unique_path();
      const std::string tempstr    = tempNameHash.native();  // optional
      
      tempFile /= tempNameHash;
      tempFile.replace_extension(".inp");
      
      // dump contents into file.
      
      ofstream modelFileStream;
      modelFileStream.open(tempFile.string());
      modelFileStream << modelContents;
      modelFileStream.close();
      
      enModel->loadModelFromFile(tempFile.string());
    } catch (std::exception &e) {
      cerr << e.what();
    }
    
  }
  else {
    cerr << "can't step sqlite -- error: " << sqlite3_errmsg(_dbHandle) << endl;
  }
  sqlite3_finalize(stmt);
  
  _model = enModel;
}




#pragma mark - point record creators
#ifndef RTX_NO_ODBC
PointRecord::sharedPointer PointRecordFactory::createOdbcRecordFromRow(sqlite3_stmt *stmt) {
  OdbcPointRecord::sharedPointer pr( new OdbcDirectPointRecord );
  return pr;
}
#endif
#ifndef RTX_NO_MYSQL
PointRecord::sharedPointer PointRecordFactory::createMysqlRecordFromRow(sqlite3_stmt *stmt) {
  MysqlPointRecord::sharedPointer pr( new MysqlPointRecord );
  return pr;
}
#endif
PointRecord::sharedPointer PointRecordFactory::createSqliteRecordFromRow(sqlite3_stmt *stmt) {
  SqlitePointRecord::sharedPointer pr( new SqlitePointRecord );
  return pr;
}



#pragma mark - timeseries creators

TimeSeries::sharedPointer SqliteProjectFile::newTimeseriesWithType(const string& type) {
  
  if (typeEquals(dbTimeseriesName)) {
    // just the base class
    TimeSeries::sharedPointer ts(new TimeSeries);
    return ts;
  }
  else if (typeEquals(dbConstantName)) {
    ConstantTimeSeries::sharedPointer ts(new ConstantTimeSeries);
    return ts;
  }
  else if (typeEquals(dbSineName)) {
    SineTimeSeries::sharedPointer ts(new SineTimeSeries);
    return ts;
  }
  else if (typeEquals(dbResamplerName)) {
    Resampler::sharedPointer ts(new Resampler);
    return ts;
  }
  else if (typeEquals(dbMovingaverageName)) {
    MovingAverage::sharedPointer ts(new MovingAverage);
    return ts;
  }
  else if (typeEquals(dbAggregatorName)) {
    AggregatorTimeSeries::sharedPointer ts(new AggregatorTimeSeries);
    return ts;
  }
  else if (typeEquals(dbDerivativeName)) {
    FirstDerivative::sharedPointer ts(new FirstDerivative);
    return ts;
  }
  else if (typeEquals(dbOffsetName)) {
    OffsetTimeSeries::sharedPointer ts(new OffsetTimeSeries);
    return ts;
  }
  else if (typeEquals(dbCurveName)) {
    CurveFunction::sharedPointer ts(new CurveFunction);
    return ts;
  }
  else if (typeEquals(dbThresholdName)) {
    ThresholdTimeSeries::sharedPointer ts(new ThresholdTimeSeries);
    return ts;
  }
  else if (typeEquals(dbValidrangeName)) {
    ValidRangeTimeSeries::sharedPointer ts(new ValidRangeTimeSeries);
    return ts;
  }
  else if (typeEquals(dbGainName)) {
    GainTimeSeries::sharedPointer ts(new GainTimeSeries);
    return ts;
  }
  else if (typeEquals(dbMultiplierName)) {
    MultiplierTimeSeries::sharedPointer ts(new MultiplierTimeSeries);
    return ts;
  }
  else if (typeEquals(dbInversionName)) {
    InversionTimeSeries::sharedPointer ts(new InversionTimeSeries);
    return ts;
  }
  else if (typeEquals(dbFailoverName)) {
    FailoverTimeSeries::sharedPointer ts(new FailoverTimeSeries);
    return ts;
  }
  else if (typeEquals(dbLagName)) {
    TimeOffsetTimeSeries::sharedPointer ts(new TimeOffsetTimeSeries);
    return ts;
  }
  else if (typeEquals(dbWarpName)) {
    WarpingTimeSeries::sharedPointer ts(new WarpingTimeSeries);
    return ts;
  }
  else if (typeEquals(dbStatsName)) {
    StatsTimeSeries::sharedPointer ts(new StatsTimeSeries);
    return ts;
  }
  else if (typeEquals(dbOutlierName)) {
    OutlierExclusionTimeSeries::sharedPointer ts(new OutlierExclusionTimeSeries);
    return ts;
  }
  
  else {
    cerr << "Did not recognize type: " << type << endl;
    return TimeSeries::sharedPointer(); // nada
  }
  
}

#pragma mark TimeSeries setters

// sets name, clock, record, units
void SqliteProjectFile::setBaseProperties(TimeSeries::sharedPointer ts, int uid) {
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(_dbHandle, sqlGetTsById.c_str(), -1, &stmt, NULL);
  sqlite3_bind_int(stmt, 1, uid);
  
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    cerr << "could not get base properties" << endl;
    return;
  }
  
  
  sqltext name =      sqlite3_column_text(stmt, 0);
  sqltext unitsText = sqlite3_column_text(stmt, 1);
  int recordUid =     sqlite3_column_int(stmt, 2);
  int clockUid =      sqlite3_column_int(stmt, 3);
  
  
  ts->setName(string((char*)name));
  
  // record
  if (recordUid > 0) { // zero is a null value; column autoincrement starts at 1
    PointRecord::sharedPointer pr = _records[recordUid];
    if (pr) {
      ts->setRecord(pr);
    }
  }
  
  // units
  string unitsStr((char*)unitsText);
  Units theUnits = Units::unitOfType(unitsStr);
  ts->setUnits(theUnits);
  
  // clock
  if (clockUid > 0) {
    Clock::sharedPointer clock = _clocks[clockUid];
    if (clock) {
      ts->setClock(clock);
    }
    else {
      cerr << "could not find clock uid " << clockUid << endl;
    }
  }
  
  sqlite3_finalize(stmt);
  
}
/*
 void SqliteProjectFile::setExtendedProperties(TimeSeries::sharedPointer ts, int uid) {
 sqlite3_stmt *stmt;
 //  sqlite3_prepare_v2(_dbHandle, sqlGetTsExtendedById.c_str(), -1, &stmt, NULL);
 sqlite3_bind_int(stmt, 1, uid);
 if (sqlite3_step(stmt) != SQLITE_ROW) {
 cerr << "could not get extended properties" << endl;
 return;
 }
 
 string type = string((char*)sqlite3_column_text(stmt, 0));
 double v1 = sqlite3_column_double(stmt, 1);
 double v2 = sqlite3_column_double(stmt, 2);
 double v3 = sqlite3_column_double(stmt, 3);
 
 //std::function<bool(string)> typeEquals = [=](string x) {return RTX_STRINGS_ARE_EQUAL(type, x);}; // woot c++11
 
 #define typeEquals(x) RTX_STRINGS_ARE_EQUAL(type,x)
 
 if (typeEquals(dbOffsetName)) {
 boost::static_pointer_cast<OffsetTimeSeries>(ts)->setOffset(v1);
 }
 else if (typeEquals(dbConstantName)) {
 boost::static_pointer_cast<ConstantTimeSeries>(ts)->setValue(v1);
 }
 else if (typeEquals(dbValidrangeName)) {
 boost::static_pointer_cast<ValidRangeTimeSeries>(ts)->setRange(v1, v2);
 ValidRangeTimeSeries::filterMode_t mode = (v3 > 0) ? ValidRangeTimeSeries::drop : ValidRangeTimeSeries::saturate;
 boost::static_pointer_cast<ValidRangeTimeSeries>(ts)->setMode(mode);
 }
 else if (typeEquals(dbResamplerName)) {
 //    boost::static_pointer_cast<Resampler>(ts)->setMode(mode);
 }
 
 }
 */



void SqliteProjectFile::setPropertyValuesForTimeSeriesWithType(TimeSeries::sharedPointer ts, const string& type, string key, double val) {
  
  
  /*** stupidly long chain of if-else statements to set k-v properties ***/
  /*** broken down first by type name, then by key name ***/
  if (typeEquals(dbTimeseriesName)) {
    // base class keys... none.
  }
  else if (typeEquals(dbConstantName)) {
    // keys: value
    if (RTX_STRINGS_ARE_EQUAL(key, "value")) {
      boost::dynamic_pointer_cast<ConstantTimeSeries>(ts)->setValue(val);
    }
  }
  else if (typeEquals(dbSineName)) {
    SineTimeSeries::sharedPointer sine = boost::dynamic_pointer_cast<SineTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "magnitude")) {
      sine->setMagnitude(val);
    }
    else if (RTX_STRINGS_ARE_EQUAL(key, "period")) {
      sine->setPeriod((time_t)val);
    }
  }
  else if (typeEquals(dbResamplerName)) {
    Resampler::sharedPointer rs = boost::dynamic_pointer_cast<Resampler>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "interpolationMode")) {
      rs->setMode((Resampler::interpolateMode_t)val);
    }
  }
  else if (typeEquals(dbMovingaverageName)) {
    MovingAverage::sharedPointer ma = boost::dynamic_pointer_cast<MovingAverage>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "window")) {
      ma->setWindowSize((int)val);
    }
  }
  else if (typeEquals(dbAggregatorName)) {
    // nothing
  }
  else if (typeEquals(dbDerivativeName)) {
    // nothing
  }
  else if (typeEquals(dbOffsetName)) {
    OffsetTimeSeries::sharedPointer os = boost::dynamic_pointer_cast<OffsetTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "offset")) {
      os->setOffset(val);
    }
  }
  else if (typeEquals(dbCurveName)) {
  }
  else if (typeEquals(dbThresholdName)) {
    ThresholdTimeSeries::sharedPointer th = boost::dynamic_pointer_cast<ThresholdTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "threshold")) {
      th->setThreshold(val);
    }
    else if (RTX_STRINGS_ARE_EQUAL(key, "value")) {
      th->setValue(val);
    }
  }
  else if (typeEquals(dbValidrangeName)) {
    ValidRangeTimeSeries::sharedPointer vr = boost::dynamic_pointer_cast<ValidRangeTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "mode")) {
      vr->setMode((ValidRangeTimeSeries::filterMode_t)val);
    }
    else if (RTX_STRINGS_ARE_EQUAL(key, "max")) {
      vr->setRange(vr->range().first, val);
    }
    else if (RTX_STRINGS_ARE_EQUAL(key, "min")) {
      vr->setRange(val, vr->range().second);
    }
  }
  else if (typeEquals(dbGainName)) {
    GainTimeSeries::sharedPointer gn = boost::dynamic_pointer_cast<GainTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "gain")) {
      gn->setGain(val);
    }
  }
  else if (typeEquals(dbMultiplierName)) {
    //
  }
  else if (typeEquals(dbInversionName)) {
    //
  }
  else if (typeEquals(dbFailoverName)) {
    FailoverTimeSeries::sharedPointer fo = boost::dynamic_pointer_cast<FailoverTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "maximumStaleness")) {
      fo->setMaximumStaleness((time_t)val);
    }
  }
  else if (typeEquals(dbLagName)) {
    TimeOffsetTimeSeries::sharedPointer os = boost::dynamic_pointer_cast<TimeOffsetTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "lag")) {
      os->setOffset((time_t)val);
    }
  }
  else if (typeEquals(dbWarpName)) {
    //
  }
  else if (typeEquals(dbStatsName)) {
    StatsTimeSeries::sharedPointer st = boost::dynamic_pointer_cast<StatsTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "samplingMode")) {
      st->setSamplingMode((StatsTimeSeries::StatsSamplingMode_t)val);
    }
    else if (RTX_STRINGS_ARE_EQUAL(key, "statsMode")) {
      st->setStatsType((StatsTimeSeries::StatsTimeSeriesType)val);
    }
  }
  else if (typeEquals(dbOutlierName)) {
    OutlierExclusionTimeSeries::sharedPointer outl = boost::dynamic_pointer_cast<OutlierExclusionTimeSeries>(ts);
    if (RTX_STRINGS_ARE_EQUAL(key, "samplingMode")) {
      outl->setSamplingMode((StatsTimeSeries::StatsSamplingMode_t)val);
    }
    else if (RTX_STRINGS_ARE_EQUAL(key, "exclusionMode")) {
      outl->setExclusionMode((OutlierExclusionTimeSeries::exclusion_mode_t)val);
    }
    else if (RTX_STRINGS_ARE_EQUAL(key, "outlierMultiplier")) {
      outl->setOutlierMultiplier(val);
    }
  }
  else {
    cerr << "unknown type name: " << type << endl;
  }
  
}



void SqliteProjectFile::setModelInputParameters() {
  
  // input params are defined by matched timeseries db-uid and model db-uid, so get that info first.
  vector<modelInputEntry> modelInputs;
  
  sqlite3_stmt *stmt;
  int ret = sqlite3_prepare_v2(_dbHandle, sqlGetModelElementParams.c_str(), -1, &stmt, NULL);
  
  if (ret != SQLITE_OK) {
    cerr << "could not prepare statement: " << sqlGetModelElementParams << endl;
    return;
  }
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    modelInputEntry entry;
    entry.tsUid = sqlite3_column_int(stmt, 0);
    entry.modelUid = sqlite3_column_int(stmt, 1);
    entry.param = string((char*)sqlite3_column_text(stmt, 2));
    modelInputs.push_back(entry);
  }
  sqlite3_finalize(stmt);
  
  ret = sqlite3_prepare_v2(_dbHandle, sqlGetModelNameAndTypeByUid.c_str(), -1, &stmt, NULL);
  if (ret != SQLITE_OK) {
    cerr << "could not prepare statement: " << sqlGetModelNameAndTypeByUid << endl;
    return;
  }
  
  BOOST_FOREACH(const modelInputEntry& entry, modelInputs) {
    // first, check if it's a valid time series
    TimeSeries::sharedPointer ts = _timeseries[entry.tsUid];
    if (!ts) {
      cerr << "Invalid time series: " << entry.tsUid << endl;
      continue;
    }
    
    // get a row for this model element
    sqlite3_bind_int(stmt, 1, entry.modelUid);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
      cerr << "could not get model element " << entry.modelUid << endl;
      continue;
    }
    // fetch the name and type of element.
    string elementName = string((char*) sqlite3_column_text(stmt, 0));
    SqliteModelParameterType elementType = (SqliteModelParameterType)sqlite3_column_int(stmt, 1);
    
    switch ((int)elementType) {
      case ParameterTypeJunction:
      case ParameterTypeTank:
      case ParameterTypeReservoir:
      {
        /// do junctioney things
        Junction::sharedPointer j = boost::dynamic_pointer_cast<Junction>(this->model()->nodeWithName(elementName));
        this->setJunctionParameter(j, entry.param, ts);
        break;
      }
      case ParameterTypePipe:
      case ParameterTypePump:
      case ParameterTypeValve:
      {
        // do pipey things
        Pipe::sharedPointer p = boost::dynamic_pointer_cast<Pipe>(this->model()->linkWithName(elementName));
        this->setPipeParameter(p, entry.param, ts);
        break;
      }
      default:
        break;
    }
    
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);
  
}


void SqliteProjectFile::setJunctionParameter(Junction::sharedPointer j, string paramName, TimeSeries::sharedPointer ts) {
  
  if (RTX_STRINGS_ARE_EQUAL(paramName, "demandBoundary")) {
    j->setBoundaryFlow(ts);
  }
  else if (RTX_STRINGS_ARE_EQUAL(paramName, "headBoundary")) {
    j->setHeadMeasure(ts);
  }
  else if (RTX_STRINGS_ARE_EQUAL(paramName, "qualityBoundary")) {
    j->setQualitySource(ts);
  }
  else if (RTX_STRINGS_ARE_EQUAL(paramName, "headMeasure")) {
    j->setHeadMeasure(ts);
  }
  else if (RTX_STRINGS_ARE_EQUAL(paramName, "levelMeasure")) {
    boost::dynamic_pointer_cast<Tank>(j)->setLevelMeasure(ts);
  }
  else if (RTX_STRINGS_ARE_EQUAL(paramName, "qualityMeasure")) {
    j->setQualityMeasure(ts);
  }
  else {
    cerr << "Unknown parameter type: " << paramName << endl;
  }
  
}

void SqliteProjectFile::setPipeParameter(Pipe::sharedPointer p, string paramName, TimeSeries::sharedPointer ts) {
  
  if (RTX_STRINGS_ARE_EQUAL(paramName, "statusBoundary")) {
    p->setStatusParameter(ts);
  }
  else if (RTX_STRINGS_ARE_EQUAL(paramName, "settingBoundary")) {
    p->setSettingParameter(ts);
  }
  else if (RTX_STRINGS_ARE_EQUAL(paramName, "flowMeasure")) {
    p->setFlowMeasure(ts);
  }
  else {
    cerr << "Unknown parameter type: " << paramName << endl;
  }
  
}

