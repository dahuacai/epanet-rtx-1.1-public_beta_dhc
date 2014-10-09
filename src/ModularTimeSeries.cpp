//
//  ModularTimeSeries.cpp
//  epanet-rtx
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  

#include <iostream>
#include <boost/foreach.hpp>
#include "ModularTimeSeries.h"

using namespace RTX;
using namespace std;

ModularTimeSeries::ModularTimeSeries() {
  _doesHaveSource = false;
}

ModularTimeSeries::~ModularTimeSeries() {
  
}

ostream& ModularTimeSeries::toStream(ostream &stream) {
  TimeSeries::toStream(stream);
  stream << "Connected to: ";
  if (_source) {
    stream << *_source << endl;
  }
  else {
    stream << "(nothing)" << endl;
  }
  return stream;
}

void ModularTimeSeries::setSource(TimeSeries::sharedPointer sourceTimeSeries) {
  if (!sourceTimeSeries) {
    _source = TimeSeries::sharedPointer();
    _doesHaveSource = false;
    return;
  }
  
  if( isCompatibleWith(sourceTimeSeries) ) {
    
    // if this is an irregular time series, then set this clock to the same as that guy's clock.
    // but only if it's irregular.
    if (!this->clock()->isRegular()) {
      setClock(sourceTimeSeries->clock());
    }
    // and if i don't have units, just borrow from the source.
    if (units().isDimensionless() && !this->canAlterDimension()) {
      setUnits(sourceTimeSeries->units());
    }
    
    _source = sourceTimeSeries;
    _doesHaveSource = true;
  }
  else {
    cerr << "Incompatible. Could not set source for:\n";
    cerr << *this;
    // TODO -- throw something?
  }
}

bool ModularTimeSeries::isCompatibleWith(TimeSeries::sharedPointer withTimeSeries) {
  
  // first section is essentially the same as base TimeSeries class
  Clock::sharedPointer theirClock = withTimeSeries->clock(), myClock = this->clock();
  bool clocksCompatible = myClock->isCompatibleWith(theirClock);
  bool unitsCompatible = units().isDimensionless() || units().isSameDimensionAs(withTimeSeries->units());
  
  // now make special allowances for specific subclass behavior
  clocksCompatible = ( clocksCompatible || this->canAlterClock() );
  unitsCompatible = ( unitsCompatible || this->canAlterDimension() );
  
  return (clocksCompatible && unitsCompatible);
}




TimeSeries::sharedPointer ModularTimeSeries::source() {
  return _source;
}

bool ModularTimeSeries::doesHaveSource() {
  if (_source) {
    return true;
  }
  else {
    return false;
  }
}


TimeSeries::sharedPointer ModularTimeSeries::rootTimeSeries() {
  
  if (this->doesHaveSource()) {
    return this->source()->rootTimeSeries();
  }
  
  TimeSeries::sharedPointer empty;
  return empty;
}



void ModularTimeSeries::setUnits(Units newUnits) {
  if (!doesHaveSource() || (doesHaveSource() && newUnits.isSameDimensionAs(source()->units())) || this->canAlterDimension()) {
    TimeSeries::setUnits(newUnits);
  }
  else {
    cerr << "could not set units for time series " << name() << endl;
  }
}
/*
bool ModularTimeSeries::isPointAvailable(time_t time) {
  bool isCacheAvailable = false, isSourceAvailable = false;
  
  isCacheAvailable = TimeSeries::isPointAvailable(time);
  
  if (!isCacheAvailable) {
    isSourceAvailable = source()->isPointAvailable(time);
  }
  
  if (isCacheAvailable || isSourceAvailable) {
    return true;
  }
  else {
    return false;
  }
}
*/
Point ModularTimeSeries::point(time_t time) {
  if (!this->source()) {
    return Point();
  }
  
  Point aPoint = TimeSeries::point(time);
  
  if (this->clock()->isRegular()) {
    
    if (!this->clock()->isValid(time)) {
      return Point();
    }
    
    if (!aPoint.isValid || aPoint.quality == Point::missing) {
      vector<Point> myPoints = filteredPoints(this->source(), time, time);
      if (myPoints.size() != 1) {
        cerr << "ERR: times not registered in modular time series" << endl;
        return Point();
      }
      aPoint = myPoints.front();
      this->insert(aPoint);
    }
  }
  
  else {
    
    if (!aPoint.isValid || aPoint.quality == Point::missing) {
      vector<Point> myPoints = filteredPoints(this->source(), time, time);
      if (myPoints.size() != 1) {
        return Point();
      }
      aPoint = myPoints.front();
      this->insert(aPoint);
    }
    
  }
  
  return aPoint;
  
  
  
  
  /*
  // check the base-class availability. if it's cached or stored here locally, then send it on.
  // otherwise, check the upstream availability. if it's there, store it locally and pass it on.
  
  Point p = TimeSeries::point(time);
  if (p.isValid) {
    return p;
  }
  else {
    
    Point sourcePoint = source()->point(time);

    if (sourcePoint.isValid) {
      // create a new point object and convert from source units
      Point aPoint = Point::convertPoint(sourcePoint, source()->units(), units());
      insert(aPoint);
      return aPoint;
    }
    else {
      //cerr << "ModularTimeSeries: check point availability first\n";
      // TODO -- throw something? reminder to check point availability first...
      Point point;
      return point;
    }
  } */
}

Point ModularTimeSeries::pointBefore(time_t time) {
  Point thePoint;
  
  time_t timeBefore = clock()->timeBefore(time);
  if (this->clock()->isRegular()) {
    thePoint = this->point(timeBefore);
  }
  else {
    if (!source()) {
      return thePoint;
    }
    timeBefore = time;
    while (timeBefore > 0 && !thePoint.isValid) {
      timeBefore = source()->pointBefore(timeBefore).time;
      thePoint = this->point(timeBefore);
    }
  }
  
  return thePoint;
}

Point ModularTimeSeries::pointAfter(time_t time) {
  Point thePoint;
  time_t timeAfter;
  if (this->clock()->isRegular()) {
    thePoint = this->point(this->clock()->timeAfter(time));
  }
  else {
    if (!source()) {
      return thePoint;
    }
    timeAfter = time; // temporary assignment to pass initial test and enter the while loop.
    while (timeAfter > 0 && !thePoint.isValid) {
      timeAfter = source()->pointAfter(timeAfter).time;
      thePoint = this->point(timeAfter);
    }
  }
  
  return thePoint;
}

vector< Point > ModularTimeSeries::points(time_t start, time_t end) {
  if (!doesHaveSource()) {
    vector<Point> empty;
    return empty;
  }
  if (!clock()->isRegular()) {
    // if the clock is irregular, there's no easy way around this.
    vector<Point> filtered = this->filteredPoints(this->source(), start, end);
    this->insertPoints(filtered);
    return filtered;
  }
  
  
  // otherwise, the clock is regular.
  // we can do some important optimizations here.
  
  // make sure the times are aligned with the clock.
  time_t newStart = (clock()->isValid(start)) ? start : clock()->timeAfter(start);
  time_t newEnd   = (clock()->isValid(end))   ? end   : clock()->timeBefore(end);
  
  PointRecord::time_pair_t prRange = record()->range(name());
  if (prRange.first <= newStart && newEnd <= prRange.second) {
    // the record's range covers it, but
    // the record may not be continuous -- so check it.
    time_t now = newStart;
    vector<Point> rpVec = record()->pointsInRange(name(), newStart, newEnd);
    if (rpVec.size() <= 1) {
      // like this:
      // ppppp---[--- req ---]---ppppp
      // pppppppp[p---req----]---ppppp
      // ppppp---[----req---p]pppppppp
      rpVec.clear();
      rpVec.push_back(record()->pointBefore(name(), newStart));
      rpVec.push_back(record()->pointAfter(name(), newEnd));
      
    }
    
    
    vector<Point> stitchedPoints;
    vector<Point>::const_iterator it = rpVec.begin();
    while (it != rpVec.end()) {
      Point recordPoint = *it;
      // cout << "P: " << recordPoint << endl;
      
      if (recordPoint.time == now) {
        stitchedPoints.push_back(recordPoint);
        now = clock()->timeAfter(now);
      }
      else if (recordPoint.time < now) {
        ++it;
        continue;
      }
      else {
        // aha, a gap.
        // determine the size of the gap
        time_t gapStart, gapEnd;
        
        gapStart = now;
        gapEnd = recordPoint.time;
        gapEnd = RTX_MIN(gapEnd, newEnd); // very large gap?
        
        vector<Point> gapPoints = filteredPoints(source(), gapStart, gapEnd);
        if (gapPoints.size() > 0) {
          this->insertPoints(gapPoints);
          now = clock()->timeAfter(gapPoints.back().time);
          BOOST_FOREACH(Point p, gapPoints) {
            stitchedPoints.push_back(p);
          }
        }
        else {
          // skipping the gap.
          now = recordPoint.time;
          continue; // skip the ++it
        }
        
      }
      
      ++it;
    }
    
    return stitchedPoints;
  }
  
  // otherwise, construct new points.
  // get the times for the source query
  Point sourceStart, sourceEnd;
  
  // create a place for the new points
  std::vector<Point> filtered;
  
  filtered = filteredPoints(source(), newStart, newEnd);
  
  // finally, add the points to myself.
  this->insertPoints(filtered);
  
  return filtered;
  
}



vector<Point> ModularTimeSeries::filteredPoints(TimeSeries::sharedPointer sourceTs, time_t fromTime, time_t toTime) {
  vector<Point> filtered;
  
  /*
   this is legacy code from resampler. it's not needed, right?
  Point fromPoint, toPoint;
  time_t fromSourceTime, toSourceTime;
  for (int i = 0; i < margin(); ++i) {
    fromPoint = sourceTs->pointBefore(fromTime);
    toPoint = sourceTs->pointAfter(toTime);
  }
  
  fromSourceTime = fromPoint.isValid ? fromPoint.time : fromTime;
  toSourceTime = toPoint.isValid ? toPoint.time : toTime;
  
  // get the source points
  std::vector<Point> sourcePoints = sourceTs->points(fromSourceTime, toSourceTime);
  */
  std::vector<Point> sourcePoints = sourceTs->points(fromTime, toTime);

//  if (sourcePoints.size() < 2) { // what's the reason not to filter a single point?
  if (sourcePoints.size() == 0) {
    return filtered;
  }
  
  BOOST_FOREACH(const Point& p, sourcePoints) {
    if (p.time < fromTime || p.time > toTime) {
      continue; // skip the points we didn't ask for.
    }
    Point aPoint = Point::convertPoint(p, sourceTs->units(), this->units());
    filtered.push_back(aPoint);
  }
  
  return filtered;
}



void ModularTimeSeries::resetCache()
{
  if (this->source()) {
    this->source()->resetCache();
  }
  TimeSeries::resetCache();
}



