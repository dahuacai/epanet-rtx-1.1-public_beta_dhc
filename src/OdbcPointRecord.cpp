//
//  OdbcPointRecord.cpp
//  epanet-rtx
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  


#include "OdbcPointRecord.h"
#include <boost/foreach.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/algorithm/string.hpp>


using namespace RTX;
using namespace std;

OdbcPointRecord::OdbcPointRecord() {
  _connectionOk = false;
  _connectorType = NO_CONNECTOR;
  _timeFormat = PointRecordTime::UTC;
  _handles.SCADAenv = NULL;
  _handles.SCADAdbc = NULL;
  
  
  SQLRETURN sqlRet;
  
  /* Allocate an environment handle */
  SQL_CHECK(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_handles.SCADAenv), "SQLAllocHandle", _handles.SCADAenv, SQL_HANDLE_ENV);
  /* We want ODBC 3 support */
  SQL_CHECK(SQLSetEnvAttr(_handles.SCADAenv, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0), "SQLSetEnvAttr", _handles.SCADAenv, SQL_HANDLE_ENV);
  /* Allocate a connection handle */
  SQL_CHECK(SQLAllocHandle(SQL_HANDLE_DBC, _handles.SCADAenv, &_handles.SCADAdbc), "SQLAllocHandle", _handles.SCADAenv, SQL_HANDLE_ENV);
  /* Connect to the DSN, checking for connectivity */
  //"Attempting to Connect to SCADA..."
  
  // readonly
  SQLUINTEGER mode = SQL_MODE_READ_ONLY;
  SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_ACCESS_MODE, &mode, SQL_IS_UINTEGER), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  
  // timeouts
  SQLUINTEGER timeout = 5;
  sqlRet = SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_LOGIN_TIMEOUT, &timeout, 0/*ignored*/), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  sqlRet = SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_CONNECTION_TIMEOUT, &timeout, 0/*ignored*/), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  

  
  
  this->initDsnList();
}


OdbcPointRecord::~OdbcPointRecord() {
  // make sure handles are free
  if (_handles.SCADAdbc != NULL) {
    SQLDisconnect(_handles.SCADAdbc);
  }
//  SQLFreeStmt(_handles.rangeStatement, SQL_CLOSE);
  SQLFreeHandle(SQL_HANDLE_DBC, _handles.SCADAdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, _handles.SCADAenv);
}

void OdbcPointRecord::initDsnList() {
  
  SQLRETURN sqlRet;
  
  _dsnList.clear();
  
  SQLCHAR dsnChar[256];
  SQLCHAR desc[256];
  SQLSMALLINT dsn_ret;
  SQLSMALLINT desc_ret;
  SQLUSMALLINT direction = SQL_FETCH_FIRST;
  while(SQL_SUCCEEDED(sqlRet = SQLDataSources(_handles.SCADAenv, direction, dsnChar, sizeof(dsnChar), &dsn_ret, desc, sizeof(desc), &desc_ret))) {
    direction = SQL_FETCH_NEXT;
    string thisDsn = string((char*)dsnChar);
    _dsnList.push_back(thisDsn);
    if (sqlRet == SQL_SUCCESS_WITH_INFO) {
      cerr << "\tdata truncation\n" << endl;
    }
  }
  
  
  
}

///! templates for selection queries
map<OdbcPointRecord::Sql_Connector_t, OdbcPointRecord::OdbcQuery> OdbcPointRecord::queryTypes() {
  map<OdbcPointRecord::Sql_Connector_t, OdbcPointRecord::OdbcQuery> list;
  
  OdbcQuery wwQueries;
  wwQueries.connectorName = "wonderware_mssql";
  wwQueries.singleSelect = "SELECT #DATECOL#, #VALUECOL#, #QUALITYCOL# FROM #TABLENAME# WHERE #TAGCOL# = ? AND (#DATECOL# = ?) AND wwTimeZone = 'UTC'";
  //wwQueries.rangeSelect =  "SELECT #DATECOL#, #TAGCOL#, #VALUECOL#, #QUALITYCOL# FROM #TABLENAME# WHERE (#DATECOL# >= ?) AND (#DATECOL# <= ?) AND #TAGCOL# = ? AND wwTimeZone = 'UTC' ORDER BY #DATECOL# asc"; // experimentally, ORDER BY is much slower. wonderware always returns rows ordered by DateTime ascending, so this is not really necessary.
  wwQueries.rangeSelect =  "SELECT #DATECOL#, #VALUECOL#, #QUALITYCOL# FROM #TABLENAME# WHERE #TAGCOL# = ? AND (#DATECOL# > ?) AND (#DATECOL# <= ?) AND wwTimeZone = 'UTC'";
  wwQueries.lowerBound = "";
  wwQueries.upperBound = "";
  wwQueries.timeQuery = "SELECT CONVERT(datetime, GETDATE()) AS DT";
  
  map<int, Point::Qual_t> wwQualMap;
  wwQualMap[192] = Point::good;
  wwQualMap[0] = Point::missing;
  
  wwQueries.qualityMap = wwQualMap;
  
  /***************************************************/
  
  OdbcQuery oraQueries;
  oraQueries.connectorName = "oracle";
  oraQueries.singleSelect = "";
  oraQueries.rangeSelect = "SELECT #DATECOL#, #VALUECOL#, #QUALITYCOL# FROM #TABLENAME# WHERE #TAGCOL# = ? AND (#DATECOL# >= ?) AND (#DATECOL# <= ?) ORDER BY #DATECOL# asc";
  oraQueries.lowerBound = "";
  oraQueries.upperBound = "";
  oraQueries.timeQuery = "select sysdate from dual";
  
  map<int, Point::Qual_t> oraQualMap;
  oraQualMap[0] = Point::good;            // 00000000000
  oraQualMap[128] = Point::questionable;  // 00010000000
  oraQualMap[192] = Point::questionable;  // 00011000000
  oraQualMap[256] = Point::questionable;  // 00100000000
  oraQualMap[768] = Point::questionable;  // 01100000000
  oraQualMap[32]   = Point::missing;      // 00000100000
  oraQualMap[1024] = Point::missing;      // 10000000000
  
  oraQueries.qualityMap = oraQualMap;
  
  
  // "regular" mssql db...
  OdbcQuery mssqlQueries = wwQueries;
  mssqlQueries.connectorName = "mssql";
  mssqlQueries.rangeSelect =  "SELECT #DATECOL#, #VALUECOL#, #QUALITYCOL# FROM #TABLENAME# WHERE #TAGCOL# = ? AND (#DATECOL# >= ?) AND (#DATECOL# <= ?)"; // ORDER BY #DATECOL# asc";
  mssqlQueries.qualityMap = oraQualMap;
  
//  mssqlQueries.lowerBound = "SELECT TOP(1) #DATECOL#, #VALUECOL#, #QUALITYCOL# FROM #TABLENAME# WHERE #TAGCOL# = ? AND (#DATECOL# < ?) ORDER BY #DATECOL# ASC";
//  mssqlQueries.upperBound = "SELECT TOP(1) #DATECOL#, #VALUECOL#, #QUALITYCOL# FROM #TABLENAME# WHERE #TAGCOL# = ? AND (#DATECOL# > ?) ORDER BY #DATECOL# DESC";
  
  
  list[mssql] = mssqlQueries;
  list[wonderware_mssql] = wwQueries;
  list[oracle] = oraQueries;
  
  return list;
}


OdbcPointRecord::Sql_Connector_t OdbcPointRecord::typeForName(const string& connector) {
  map<OdbcPointRecord::Sql_Connector_t, OdbcPointRecord::OdbcQuery> list = queryTypes();
  
  BOOST_FOREACH(Sql_Connector_t connType, list | boost::adaptors::map_keys) {
    if (RTX_STRINGS_ARE_EQUAL(connector, list[connType].connectorName)) {
      return connType;
    }
  }

  cerr << "could not resolve connector type: " << connector << endl;
  return NO_CONNECTOR;
}


vector<string> OdbcPointRecord::dsnList() {
  return _dsnList;
}



OdbcPointRecord::Sql_Connector_t OdbcPointRecord::connectorType() {
  return _connectorType;
}


#pragma mark -

void OdbcPointRecord::setConnectorType(Sql_Connector_t connectorType) {
  
  _connectorType = connectorType;
  
  if (connectorType == NO_CONNECTOR) {
    return;
  }
  
  this->rebuildQueries();
  

}


void OdbcPointRecord::rebuildQueries() {
  
  map<OdbcPointRecord::Sql_Connector_t, OdbcPointRecord::OdbcQuery> qTypes = queryTypes();
  if (qTypes.find(this->connectorType()) == qTypes.end()) {
    cerr << "OdbcPointRecord could not find the specified connector type" << endl;
    return;
  }
  
  _querySyntax = qTypes[this->connectorType()];
  
  {
    // do some string replacement to construct the sql queries from my type's template queries.
    vector<string*> querystrings;
    querystrings.push_back(&_querySyntax.singleSelect);
    querystrings.push_back(&_querySyntax.rangeSelect);
    querystrings.push_back(&_querySyntax.upperBound);
    querystrings.push_back(&_querySyntax.lowerBound);
    
    BOOST_FOREACH(string* str, querystrings) {
      boost::replace_all(*str, "#TABLENAME#",  tableDescription.dataTable);
      boost::replace_all(*str, "#DATECOL#",    tableDescription.dataDateCol);
      boost::replace_all(*str, "#TAGCOL#",     tableDescription.dataNameCol);
      boost::replace_all(*str, "#VALUECOL#",   tableDescription.dataValueCol);
      boost::replace_all(*str, "#QUALITYCOL#", tableDescription.dataQualityCol);
    }
  }
  
  
  _query.tagNameInd = SQL_NTS;
  
}
//DHC add ......
// select just returns the results (no caching)
vector<Point> OdbcPointRecord::selectRange(const string& id, time_t startTime, time_t endTime) {
	vector<Point> p;
	return p;
}


Point OdbcPointRecord::selectNext(const string& id, time_t time) {
	Point p;
	//time_t margin = 60*60*12;
	//vector<Point> points = pointsWithStatement(id, _handles.rangeStatement, time-1, time + margin);

	//time_t max_margin = this->searchDistance();
	//time_t lookahead = time;
	//while (points.size() == 0 && lookahead < time + max_margin) {
	//	//cout << "scada lookahead" << endl;
	//	lookahead += margin;
	//	points = pointsWithStatement(id, _handles.rangeStatement, time-1, lookahead+margin);
	//}

	//if (points.size() > 0) {
	//	p = points.front();
	//	int i = 0;
	//	while (p.time <= time && i < points.size()) {
	//		p = points.at(i);
	//		++i;
	//	}
	//}
	//if (points.empty()) {
	//	//cerr << "no points found for " << id << " :: range " << time - 1 << " - " << lookahead + margin << endl;
	//}
	return p;
}


Point OdbcPointRecord::selectPrevious(const string& id, time_t time) {
	Point p;
	//time_t margin = 60*60*24*7; // one week?
	//vector<Point> points = pointsWithStatement(id, _handles.rangeStatement, time - margin, time+1);

	//time_t max_margin = this->searchDistance();
	//time_t lookbehind = time;
	//while (points.size() == 0 && lookbehind > time - max_margin) {
	//	//cout << "scada lookbehind" << endl;
	//	lookbehind -= margin;
	//	points = pointsWithStatement(id, _handles.rangeStatement, lookbehind-margin, time+1);
	//}
	//// sanity
	//while (!points.empty() && points.back().time > time) {
	//	points.pop_back();
	//}

	//if (points.size() > 0) {
	//	p = points.back();
	//}
	//if (points.empty()) {
	//	//cerr << "no points found for " << id << " :: range " << lookbehind-margin << " - " << time+1 << endl;
	//}
	return p;
}
//DHC add ......
void OdbcPointRecord::dbConnect() throw(RtxException) {
  
  if (_connectionOk) {
    SQLDisconnect(_handles.SCADAdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, _handles.SCADAdbc);
    /* Allocate a connection handle */
    SQL_CHECK(SQLAllocHandle(SQL_HANDLE_DBC, _handles.SCADAenv, &_handles.SCADAdbc), "SQLAllocHandle", _handles.SCADAenv, SQL_HANDLE_ENV);
    SQLUINTEGER mode = SQL_MODE_READ_ONLY;
    SQL_CHECK(SQLSetConnectAttr(_handles.SCADAdbc, SQL_ATTR_ACCESS_MODE, &mode, SQL_IS_UINTEGER), "SQLSetConnectAttr", _handles.SCADAdbc, SQL_HANDLE_DBC);
  }
  
  _connectionOk = false;
  if (RTX_STRINGS_ARE_EQUAL(this->connection.dsn, "") ||
      RTX_STRINGS_ARE_EQUAL(this->connection.uid, "") ||
      RTX_STRINGS_ARE_EQUAL(this->connection.pwd, "") ) {
    errorMessage = "Incomplete Login";
    return;
  }
  
  this->rebuildQueries();
  
  SQLRETURN sqlRet;
  
  try {
    sqlRet = SQLConnect(_handles.SCADAdbc, (SQLCHAR*)this->connection.dsn.c_str(), SQL_NTS, (SQLCHAR*)this->connection.uid.c_str(), SQL_NTS, (SQLCHAR*)this->connection.pwd.c_str(), SQL_NTS);
    SQL_CHECK(sqlRet, "SQLDriverConnect", _handles.SCADAdbc, SQL_HANDLE_DBC);
    _connectionOk = true;
    errorMessage = "Connected";
    
  } catch (string err) {
    errorMessage = err;
    cerr << "Initialize failed: " << err << "\n";
    _connectionOk = false;
    //throw DbPointRecord::RtxDbConnectException();
  }
  
  // todo -- check time offset (dst?)
}

bool OdbcPointRecord::isConnected() {
  return _connectionOk;
}

bool OdbcPointRecord::checkConnected() {
  if (!_connectionOk) {
    this->dbConnect();
  }
  return _connectionOk;
}

string OdbcPointRecord::registerAndGetIdentifier(string recordName, Units dataUnits) {
  return DB_PR_SUPER::registerAndGetIdentifier(recordName, dataUnits);
}

vector<string> OdbcPointRecord::identifiers() {
  vector<string> ids;
  return ids;
}



#pragma mark - Protected

ostream& OdbcPointRecord::toStream(ostream &stream) {
  stream << "ODBC Scada Point Record" << endl;
  // todo - stream extra info
  return stream;
}




void OdbcPointRecord::bindOutputColumns(SQLHSTMT statement, ScadaRecord* record) {
  SQL_CHECK(SQLBindCol(statement, 1, SQL_TYPE_TIMESTAMP, &(record->time), NULL, &(record->timeInd) ), "SQLBindCol", statement, SQL_HANDLE_STMT);
  SQL_CHECK(SQLBindCol(statement, 2, SQL_DOUBLE, &(record->value), 0, &(record->valueInd) ), "SQLBindCol", statement, SQL_HANDLE_STMT);
  SQL_CHECK(SQLBindCol(statement, 3, SQL_INTEGER, &(record->quality), 0, &(record->qualityInd) ), "SQLBindCol", statement, SQL_HANDLE_STMT);
}




#pragma mark - Internal (private) methods

// get a collection of points out of the db using an active statement handle.
// it is the caller's responsibility to execute something on the passed-in handle before calling this method.

std::vector<Point> OdbcPointRecord::pointsFromStatement(SQLHSTMT statement) {
  vector<ScadaRecord> records;
  vector<Point> points;
  
  // make sure output columns are bound. we're not sure where this statement is coming from.
  this->bindOutputColumns(statement, &_tempRecord);
  
  try {
    if (statement == NULL) {
      throw string("Connection not initialized.");
    }
    while (SQL_SUCCEEDED(SQLFetch(statement))) {
      records.push_back(_tempRecord);
    }
    SQL_CHECK(SQLFreeStmt(statement, SQL_CLOSE), "SQLCancel", statement, SQL_HANDLE_STMT);
  }
  catch(string errorMessage) {
    cerr << errorMessage << endl;
    cerr << "Could not get data from db connection\n";
    cerr << "Attempting to reconnect..." << endl;
    this->dbConnect();
    cerr << "Connection returned " << this->isConnected() << endl;
  }
  
  BOOST_FOREACH(const ScadaRecord& record, records) {
    Point p;
    //time_t t = unixTime(record.time);
    time_t t = PointRecordTime::time(record.time);
    double v = record.value;
    int qu = record.quality;
    Point::Qual_t q = Point::missing;
    
    // map to rtx quality types
    if (_querySyntax.qualityMap.count(qu) > 0) {
      q = _querySyntax.qualityMap[qu];
    }
    else {
      q = Point::bad;
    }
    
    if (record.valueInd > 0 && !(q & Point::missing)) {
      // ok
      p = Point(t, v, q);
      points.push_back(p);
    }
    else {
      // nothing
      //cout << "skipped invalid point. quality = " << _tempRecord.quality << endl;
    }
  }
  
  // make sure the points are sorted
  std::sort(points.begin(), points.end(), &Point::comparePointTime);
  
  return points;
}


bool OdbcPointRecord::supportsBoundedQueries() {
  return (!RTX_STRINGS_ARE_EQUAL(_querySyntax.upperBound, "") && !RTX_STRINGS_ARE_EQUAL(_querySyntax.lowerBound, ""));
}



SQLRETURN OdbcPointRecord::SQL_CHECK(SQLRETURN retVal, string function, SQLHANDLE handle, SQLSMALLINT type) throw(string)
{
	if(!SQL_SUCCEEDED(retVal)) {
    string errorMessage;
		errorMessage = extract_error(function, handle, type);
    throw errorMessage;
	}
	return retVal;
}


string OdbcPointRecord::extract_error(string function, SQLHANDLE handle, SQLSMALLINT type)
{
  SQLINTEGER	 i = 0;
  SQLINTEGER	 native;
  SQLCHAR	 state[ 7 ];
  SQLCHAR	 text[256];
  SQLSMALLINT	 len;
  SQLRETURN	 ret;
  string msg("");
  
  do
  {
    ret = SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len );
    if (SQL_SUCCEEDED(ret)) {
      msg += (char*)state;
      msg += "::";
      msg += (long int)i;
      msg += "::";
      msg += (long int)native;
      msg += "::";
      msg += (char*)text;
    }
    errorMessage = std::string((char*)text);
    // check if it's a connection issue
    if (strncmp((char*)state, "SCADA_CONNECTION_ISSSUE", 2) == 0) {
      msg += "::Connection Issue::";
      return msg;
    }
  }
  while( ret == SQL_SUCCESS );
	return msg;
}


