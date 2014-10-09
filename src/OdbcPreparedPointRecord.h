//
//  OdbcPreparedPointRecord.h
//  epanet-rtx
//
//  Open Water Analytics [wateranalytics.org]
//  See README.md and license.txt for more information
//

#ifndef __epanet_rtx__OdbcPreparedPointRecord__
#define __epanet_rtx__OdbcPreparedPointRecord__
#include <iostream>
#include "OdbcPointRecord.h"//#include "OdbcPreparedPointRecord.h"//dhc modify for error included
namespace RTX {
  class OdbcPreparedPointRecord : public OdbcPointRecord {
  public:
	  RTX_SHARED_POINTER(OdbcPreparedPointRecord);
	   OdbcPreparedPointRecord();
	   virtual ~OdbcPreparedPointRecord();
	   void initDsnList();
	   void dbConnect();
	   std::vector<std::string>  identifiers();
	  std:: vector<string>dsnList();
	   std::map<OdbcPointRecord::Sql_Connector_t, OdbcPointRecord::OdbcQuery>queryTypes();
	   OdbcPointRecord::Sql_Connector_t typeForName(const string& connector);
     
	   OdbcTableDescription _tableDescription;

	   // abstract stubs
	   void rebuildQueries(); // must call base
	   std::vector<Point> selectRange(const std::string& id, time_t startTime, time_t endTime);
	   Point selectNext(const std::string& id, time_t time);
	   Point selectPrevious(const std::string& id, time_t time);



	   // insertions or alterations may choose to ignore / deny
	   // pseudo-abstract base is no-op
	    void insertSingle(const std::string& id, Point point);
	    void insertRange(const std::string& id, std::vector<Point> points);
	    void removeRecord(const std::string& id);
	    void truncate();

protected :

	std::ostream& toStream(std::ostream &stream);
  private:
	  string _dsn;
	  string _pwd;
	  string _uid;

	  vector<string> _dsnList;
	  bool _connectionOk;

	  PointRecordTime::time_format_t _timeFormat;
	  Sql_Connector_t _connectorType;

	  std::string OdbcPreparedPointRecord::dsn();
	  std::string OdbcPreparedPointRecord::uid();
	  std::string OdbcPreparedPointRecord::pwd();

	 std::vector<Point>pointsWithStatement(const string& id, SQLHSTMT statement, time_t startTime, time_t endTime);
    void bindOutputColumns(SQLHSTMT statement, ScadaRecord* record);
    std::string stringQueryForRange(const std::string& id, time_t start, time_t end);
    SQL_TIMESTAMP_STRUCT sqlTime(time_t unixTime);
    time_t sql_to_time_t( const SQL_TIMESTAMP_STRUCT& sqlTime );
    time_t boost_convert_tm_to_time_t(const struct tm &tmStruct) ;
    SQLRETURN SQL_CHECK(SQLRETURN retVal, string function, SQLHANDLE handle, SQLSMALLINT type) throw(string);
    string extract_error(string function, SQLHANDLE handle, SQLSMALLINT type);

   };
}



#endif /* defined(__epanet_rtx__OdbcPreparedPointRecord__) */
