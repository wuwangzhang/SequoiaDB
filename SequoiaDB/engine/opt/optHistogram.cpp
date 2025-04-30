/*******************************************************************************


   Copyright (C) 2023-present SequoiaDB Ltd.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Source File Name = optHistogram.cpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#include "optHistogram.hpp"
#include "pd.hpp"
#include "ossUtil.hpp"
#include "pmdEDU.hpp"
#include "rtn.hpp"
#include "rtnContextData.hpp"
#include "rtnCommand.hpp"
#include "rtnCommandDef.hpp"
#include "pmd.hpp"
#include "pmdCB.hpp"
#include "rtnCoordCommon.hpp"
#include "msgMessage.hpp"
#include "rtnLocalCoordinator.hpp"
#include "coordCB.hpp"

namespace engine
{
   /*
    * _optHistogramBucket implement
    */
   _optHistogramBucket::_optHistogramBucket()
   : _count( 0 ),
     _type( OPT_HISTOGRAM_BUCKET_SINGLETON )
   {
   }

   _optHistogramBucket::_optHistogramBucket( const BSONElement &lowerBound, 
                                             const BSONElement &upperBound,
                                             UINT64 count,
                                             OPT_HISTOGRAM_BUCKET_TYPE type )
   : _count( count ),
     _type( type )
   {
      _ownedLower = BSON( "v" << lowerBound ) ;
      _ownedUpper = BSON( "v" << upperBound ) ;
      _lowerBound = _ownedLower.getField( "v" ) ;
      _upperBound = _ownedUpper.getField( "v" ) ;
   }

   _optHistogramBucket::~_optHistogramBucket()
   {
   }

   BOOLEAN _optHistogramBucket::contains( const BSONElement &value ) const
   {
      if ( isSingleton() )
      {
         return _lowerBound.woCompare( value, false ) == 0 ;
      }
      else
      {
         return _lowerBound.woCompare( value, false ) <= 0 &&
                _upperBound.woCompare( value, false ) >= 0 ;
      }
   }

   double _optHistogramBucket::calculateSelectivity( const BSONElement &value, 
                                                     UINT64 totalCount ) const
   {
      if ( totalCount == 0 )
      {
         return 0.0 ;
      }

      if ( !contains( value ) )
      {
         return 0.0 ;
      }

      if ( isSingleton() )
      {
         return (double)_count / (double)totalCount ;
      }
      else
      {
         return (double)_count / (double)totalCount ;
      }
   }

   double _optHistogramBucket::calculateRangeSelectivity( 
                                        const BSONElement &lower, 
                                        BOOLEAN lowerIncluded,
                                        const BSONElement &upper, 
                                        BOOLEAN upperIncluded,
                                        UINT64 totalCount ) const
   {
      if ( totalCount == 0 )
      {
         return 0.0 ;
      }

      int lowerCmp = _lowerBound.woCompare( upper, false ) ;
      int upperCmp = _upperBound.woCompare( lower, false ) ;

      if ( lowerCmp > 0 || upperCmp < 0 )
      {
         return 0.0 ;
      }

      if ( !upperIncluded && upperCmp == 0 )
      {
         return 0.0 ;
      }

      if ( !lowerIncluded && lowerCmp == 0 )
      {
         return 0.0 ;
      }

      if ( isSingleton() )
      {
         return (double)_count / (double)totalCount ;
      }
      else
      {
         double overlapRatio = 1.0 ;
         
         if ( _lowerBound.woCompare( lower, false ) < 0 &&
              _upperBound.woCompare( upper, false ) > 0 )
         {
            overlapRatio = 1.0 ;
         }
         else if ( _lowerBound.woCompare( lower, false ) >= 0 &&
                   _upperBound.woCompare( upper, false ) <= 0 )
         {
            overlapRatio = 0.5 ;
         }
         else
         {
            overlapRatio = 0.5 ;
         }

         return (double)_count * overlapRatio / (double)totalCount ;
      }
   }

   BSONObj _optHistogramBucket::toBSON() const
   {
      BSONObjBuilder builder ;
      
      builder.append( "lower", _lowerBound ) ;
      builder.append( "upper", _upperBound ) ;
      builder.append( "count", (long long)_count ) ;
      builder.append( "type", (int)_type ) ;
      
      return builder.obj() ;
   }

   _optHistogramBucket *_optHistogramBucket::fromBSON( const BSONObj &obj )
   {
      optHistogramBucket *bucket = NULL ;
      
      try
      {
         BSONElement lowerElem = obj.getField( "lower" ) ;
         BSONElement upperElem = obj.getField( "upper" ) ;
         long long count = obj.getField( "count" ).numberLong() ;
         int type = obj.getField( "type" ).numberInt() ;
         
         bucket = SDB_OSS_NEW optHistogramBucket( 
                                 lowerElem, upperElem, 
                                 (UINT64)count, 
                                 (OPT_HISTOGRAM_BUCKET_TYPE)type ) ;
         if ( !bucket )
         {
            PD_LOG( PDERROR, "Failed to allocate memory for histogram bucket" ) ;
            goto error ;
         }
      }
      catch ( std::exception &e )
      {
         PD_LOG( PDERROR, "Exception occurred while parsing histogram bucket: %s",
                 e.what() ) ;
         goto error ;
      }
      
      return bucket ;
      
   error:
      if ( bucket )
      {
         SDB_OSS_DEL bucket ;
         bucket = NULL ;
      }
      return NULL ;
   }

   /*
    * _optHistogram implement
    */
   _optHistogram::_optHistogram()
   : _totalCount( 0 ),
     _maxBuckets( OPT_HISTOGRAM_DEFAULT_BUCKETS )
   {
   }

   _optHistogram::_optHistogram( const CHAR *fieldName, UINT32 numBuckets )
   : _fieldName( fieldName ),
     _totalCount( 0 ),
     _maxBuckets( numBuckets )
   {
   }

   _optHistogram::~_optHistogram()
   {
      for ( UINT32 i = 0; i < _buckets.size(); i++ )
      {
         SDB_OSS_DEL _buckets[i] ;
      }
      _buckets.clear() ;
   }

   INT32 _optHistogram::addBucket( optHistogramBucket *bucket )
   {
      if ( !bucket )
      {
         return SDB_INVALIDARG ;
      }
      
      _buckets.push_back( bucket ) ;
      _totalCount += bucket->getCount() ;
      
      return SDB_OK ;
   }

   INT32 _optHistogram::buildFromSample( const vector<BSONObj> &sample )
   {
      INT32 rc = SDB_OK ;
      
      if ( sample.empty() )
      {
         rc = SDB_INVALIDARG ;
         PD_LOG( PDERROR, "Sample is empty" ) ;
         goto error ;
      }
      
      try
      {
         for ( UINT32 i = 0; i < _buckets.size(); i++ )
         {
            SDB_OSS_DEL _buckets[i] ;
         }
         _buckets.clear() ;
         _totalCount = 0 ;
         
         vector<BSONElement> values ;
         for ( UINT32 i = 0; i < sample.size(); i++ )
         {
            BSONElement elem = sample[i].getFieldDotted( _fieldName.c_str() ) ;
            if ( !elem.eoo() )
            {
               values.push_back( elem ) ;
            }
         }
         
         if ( values.empty() )
         {
            PD_LOG( PDWARNING, "No values found for field %s in sample",
                    _fieldName.c_str() ) ;
            goto done ;
         }
         
         map<BSONObj, UINT64> valueCounts ;
         for ( UINT32 i = 0; i < values.size(); i++ )
         {
            BSONObj valueObj = BSON( "v" << values[i] ) ;
            valueCounts[valueObj]++ ;
         }
         
         if ( valueCounts.size() <= _maxBuckets )
         {
            for ( map<BSONObj, UINT64>::iterator it = valueCounts.begin();
                  it != valueCounts.end(); ++it )
            {
               BSONElement elem = it->first.getField( "v" ) ;
               optHistogramBucket *bucket = SDB_OSS_NEW optHistogramBucket(
                                               elem, elem, it->second,
                                               OPT_HISTOGRAM_BUCKET_SINGLETON ) ;
               if ( !bucket )
               {
                  rc = SDB_OOM ;
                  PD_LOG( PDERROR, "Failed to allocate memory for histogram bucket" ) ;
                  goto error ;
               }
               
               rc = addBucket( bucket ) ;
               if ( rc )
               {
                  SDB_OSS_DEL bucket ;
                  PD_LOG( PDERROR, "Failed to add bucket to histogram, rc: %d", rc ) ;
                  goto error ;
               }
            }
         }
         else
         {
            vector<pair<BSONObj, UINT64> > valueCountVec ;
            for ( map<BSONObj, UINT64>::iterator it = valueCounts.begin();
                  it != valueCounts.end(); ++it )
            {
               valueCountVec.push_back( make_pair( it->first, it->second ) ) ;
            }
            
            UINT32 valuesPerBucket = (valueCountVec.size() + _maxBuckets - 1) / _maxBuckets ;
            for ( UINT32 i = 0; i < valueCountVec.size(); i += valuesPerBucket )
            {
               UINT32 endIdx = min( i + valuesPerBucket, (UINT32)valueCountVec.size() ) ;
               BSONElement lowerElem = valueCountVec[i].first.getField( "v" ) ;
               BSONElement upperElem = valueCountVec[endIdx - 1].first.getField( "v" ) ;
               
               UINT64 bucketCount = 0 ;
               for ( UINT32 j = i; j < endIdx; j++ )
               {
                  bucketCount += valueCountVec[j].second ;
               }
               
               optHistogramBucket *bucket = SDB_OSS_NEW optHistogramBucket(
                                               lowerElem, upperElem, bucketCount,
                                               OPT_HISTOGRAM_BUCKET_RANGE ) ;
               if ( !bucket )
               {
                  rc = SDB_OOM ;
                  PD_LOG( PDERROR, "Failed to allocate memory for histogram bucket" ) ;
                  goto error ;
               }
               
               rc = addBucket( bucket ) ;
               if ( rc )
               {
                  SDB_OSS_DEL bucket ;
                  PD_LOG( PDERROR, "Failed to add bucket to histogram, rc: %d", rc ) ;
                  goto error ;
               }
            }
         }
      }
      catch ( std::exception &e )
      {
         rc = SDB_SYS ;
         PD_LOG( PDERROR, "Exception occurred while building histogram: %s",
                 e.what() ) ;
         goto error ;
      }
      
   done:
      return rc ;
   error:
      goto done ;
   }

   double _optHistogram::calculateSelectivity( const BSONElement &value ) const
   {
      double selectivity = 0.0 ;
      
      for ( UINT32 i = 0; i < _buckets.size(); i++ )
      {
         selectivity += _buckets[i]->calculateSelectivity( value, _totalCount ) ;
      }
      
      return selectivity ;
   }

   double _optHistogram::calculateRangeSelectivity( const BSONElement &lower, 
                                                    BOOLEAN lowerIncluded,
                                                    const BSONElement &upper, 
                                                    BOOLEAN upperIncluded ) const
   {
      double selectivity = 0.0 ;
      
      for ( UINT32 i = 0; i < _buckets.size(); i++ )
      {
         selectivity += _buckets[i]->calculateRangeSelectivity( 
                           lower, lowerIncluded, upper, upperIncluded, _totalCount ) ;
      }
      
      return selectivity ;
   }

   double _optHistogram::calculateJoinSelectivity( 
                           const _optHistogram &otherHistogram ) const
   {
      if ( _buckets.empty() || otherHistogram.getBuckets().empty() )
      {
         return OPT_JOIN_DEFAULT_SELECTIVITY ;
      }
      
      UINT32 distinctValues1 = _buckets.size() ;
      UINT32 distinctValues2 = otherHistogram.getNumBuckets() ;
      
      double selectivity = 1.0 / (double)max( distinctValues1, distinctValues2 ) ;
      
      double correctionFactor = 1.0 ;
      
      selectivity *= correctionFactor ;
      
      selectivity = max( selectivity, OPT_JOIN_MIN_SELECTIVITY ) ;
      selectivity = min( selectivity, OPT_JOIN_MAX_SELECTIVITY ) ;
      
      return selectivity ;
   }

   BSONObj _optHistogram::toBSON() const
   {
      BSONObjBuilder builder ;
      
      builder.append( "field", _fieldName ) ;
      builder.append( "totalCount", (long long)_totalCount ) ;
      builder.append( "maxBuckets", (int)_maxBuckets ) ;
      
      BSONArrayBuilder bucketsBuilder ;
      for ( UINT32 i = 0; i < _buckets.size(); i++ )
      {
         bucketsBuilder.append( _buckets[i]->toBSON() ) ;
      }
      
      builder.append( "buckets", bucketsBuilder.arr() ) ;
      
      return builder.obj() ;
   }

   _optHistogram *_optHistogram::fromBSON( const BSONObj &obj )
   {
      optHistogram *histogram = NULL ;
      
      try
      {
         string fieldName = obj.getStringField( "field" ) ;
         int maxBuckets = obj.getIntField( "maxBuckets" ) ;
         
         histogram = SDB_OSS_NEW optHistogram( fieldName.c_str(), maxBuckets ) ;
         if ( !histogram )
         {
            PD_LOG( PDERROR, "Failed to allocate memory for histogram" ) ;
            goto error ;
         }
         
         histogram->_totalCount = obj.getField( "totalCount" ).numberLong() ;
         
         BSONElement bucketsElem = obj.getField( "buckets" ) ;
         if ( bucketsElem.type() == Array )
         {
            vector<BSONElement> bucketElems ;
            bucketsElem.Obj().elems( bucketElems ) ;
            
            for ( UINT32 i = 0; i < bucketElems.size(); i++ )
            {
               optHistogramBucket *bucket = optHistogramBucket::fromBSON( 
                                               bucketElems[i].Obj() ) ;
               if ( !bucket )
               {
                  PD_LOG( PDERROR, "Failed to parse histogram bucket" ) ;
                  goto error ;
               }
               
               INT32 rc = histogram->addBucket( bucket ) ;
               if ( rc )
               {
                  SDB_OSS_DEL bucket ;
                  PD_LOG( PDERROR, "Failed to add bucket to histogram, rc: %d", rc ) ;
                  goto error ;
               }
            }
         }
      }
      catch ( std::exception &e )
      {
         PD_LOG( PDERROR, "Exception occurred while parsing histogram: %s",
                 e.what() ) ;
         goto error ;
      }
      
      return histogram ;
      
   error:
      if ( histogram )
      {
         SDB_OSS_DEL histogram ;
         histogram = NULL ;
      }
      return NULL ;
   }

   /*
    * _optHistogramManager implement
    */
   _optHistogramManager::_optHistogramManager()
   {
   }

   _optHistogramManager::~_optHistogramManager()
   {
      for ( optHistogramMap::iterator it = _histograms.begin();
            it != _histograms.end(); ++it )
      {
         SDB_OSS_DEL it->second ;
      }
      _histograms.clear() ;
   }

   optHistogram *_optHistogramManager::getHistogram( const CHAR *fieldName ) const
   {
      if ( !fieldName )
      {
         return NULL ;
      }
      
      optHistogramMap::const_iterator it = _histograms.find( fieldName ) ;
      if ( it != _histograms.end() )
      {
         return it->second ;
      }
      
      return NULL ;
   }

   INT32 _optHistogramManager::addHistogram( optHistogram *histogram )
   {
      if ( !histogram )
      {
         return SDB_INVALIDARG ;
      }
      
      const CHAR *fieldName = histogram->getFieldName() ;
      if ( !fieldName )
      {
         return SDB_INVALIDARG ;
      }
      
      optHistogramMap::iterator it = _histograms.find( fieldName ) ;
      if ( it != _histograms.end() )
      {
         SDB_OSS_DEL it->second ;
         it->second = histogram ;
      }
      else
      {
         _histograms[fieldName] = histogram ;
      }
      
      return SDB_OK ;
   }

   INT32 _optHistogramManager::buildFromSample( const vector<BSONObj> &sample,
                                                const vector<string> &fields,
                                                UINT32 numBuckets )
   {
      INT32 rc = SDB_OK ;
      
      if ( sample.empty() || fields.empty() )
      {
         rc = SDB_INVALIDARG ;
         PD_LOG( PDERROR, "Sample or fields is empty" ) ;
         goto error ;
      }
      
      try
      {
         for ( UINT32 i = 0; i < fields.size(); i++ )
         {
            optHistogram *histogram = SDB_OSS_NEW optHistogram( 
                                         fields[i].c_str(), numBuckets ) ;
            if ( !histogram )
            {
               rc = SDB_OOM ;
               PD_LOG( PDERROR, "Failed to allocate memory for histogram" ) ;
               goto error ;
            }
            
            rc = histogram->buildFromSample( sample ) ;
            if ( rc )
            {
               SDB_OSS_DEL histogram ;
               PD_LOG( PDERROR, "Failed to build histogram from sample, rc: %d", rc ) ;
               goto error ;
            }
            
            rc = addHistogram( histogram ) ;
            if ( rc )
            {
               SDB_OSS_DEL histogram ;
               PD_LOG( PDERROR, "Failed to add histogram to manager, rc: %d", rc ) ;
               goto error ;
            }
         }
      }
      catch ( std::exception &e )
      {
         rc = SDB_SYS ;
         PD_LOG( PDERROR, "Exception occurred while building histograms: %s",
                 e.what() ) ;
         goto error ;
      }
      
   done:
      return rc ;
   error:
      goto done ;
   }

   double _optHistogramManager::calculateJoinSelectivity( 
                                 const CHAR *leftField,
                                 const CHAR *rightField,
                                 const _optHistogramManager &rightHistMgr ) const
   {
      if ( !leftField || !rightField )
      {
         return OPT_JOIN_DEFAULT_SELECTIVITY ;
      }
      
      optHistogram *leftHistogram = getHistogram( leftField ) ;
      optHistogram *rightHistogram = rightHistMgr.getHistogram( rightField ) ;
      
      if ( !leftHistogram || !rightHistogram )
      {
         return OPT_JOIN_DEFAULT_SELECTIVITY ;
      }
      
      return leftHistogram->calculateJoinSelectivity( *rightHistogram ) ;
   }

   BSONObj _optHistogramManager::toBSON() const
   {
      BSONObjBuilder builder ;
      
      BSONArrayBuilder histogramsBuilder ;
      for ( optHistogramMap::const_iterator it = _histograms.begin();
            it != _histograms.end(); ++it )
      {
         histogramsBuilder.append( it->second->toBSON() ) ;
      }
      
      builder.append( "histograms", histogramsBuilder.arr() ) ;
      
      return builder.obj() ;
   }

   _optHistogramManager *_optHistogramManager::fromBSON( const BSONObj &obj )
   {
      optHistogramManager *manager = NULL ;
      
      try
      {
         manager = SDB_OSS_NEW optHistogramManager() ;
         if ( !manager )
         {
            PD_LOG( PDERROR, "Failed to allocate memory for histogram manager" ) ;
            goto error ;
         }
         
         BSONElement histogramsElem = obj.getField( "histograms" ) ;
         if ( histogramsElem.type() == Array )
         {
            vector<BSONElement> histogramElems ;
            histogramsElem.Obj().elems( histogramElems ) ;
            
            for ( UINT32 i = 0; i < histogramElems.size(); i++ )
            {
               optHistogram *histogram = optHistogram::fromBSON( 
                                            histogramElems[i].Obj() ) ;
               if ( !histogram )
               {
                  PD_LOG( PDERROR, "Failed to parse histogram" ) ;
                  goto error ;
               }
               
               INT32 rc = manager->addHistogram( histogram ) ;
               if ( rc )
               {
                  SDB_OSS_DEL histogram ;
                  PD_LOG( PDERROR, "Failed to add histogram to manager, rc: %d", rc ) ;
                  goto error ;
               }
            }
         }
      }
      catch ( std::exception &e )
      {
         PD_LOG( PDERROR, "Exception occurred while parsing histogram manager: %s",
                 e.what() ) ;
         goto error ;
      }
      
      return manager ;
      
   error:
      if ( manager )
      {
         SDB_OSS_DEL manager ;
         manager = NULL ;
      }
      return NULL ;
   }

   /*
    * _optDynamicSampler implement
    */
   _optDynamicSampler::_optDynamicSampler()
   {
   }

   _optDynamicSampler::~_optDynamicSampler()
   {
   }

   INT32 _optDynamicSampler::takeSample( const CHAR *collectionName,
                                         UINT32 sampleSize,
                                         vector<BSONObj> &sample )
   {
      INT32 rc = SDB_OK ;
      
      if ( !collectionName || sampleSize == 0 )
      {
         rc = SDB_INVALIDARG ;
         PD_LOG( PDERROR, "Invalid arguments for takeSample" ) ;
         goto error ;
      }
      
      try
      {
         sample.clear() ;
         
         BSONObj query = BSON( "" << 1 ) ;
         BSONObj selector ;
         BSONObj orderBy ;
         BSONObj hint ;
         
         pmdKRCB *krcb = pmdGetKRCB() ;
         SDB_RTNCB *rtnCB = krcb->getRTNCB() ;
         
         rtnContextBuf buffObj ;
         
         rc = rtnQuery( collectionName, selector, query, orderBy, hint,
                        0, 0, sampleSize, buffObj, NULL, NULL ) ;
         if ( rc )
         {
            PD_LOG( PDERROR, "Failed to execute query for sampling, rc: %d", rc ) ;
            goto error ;
         }
         
         while ( !buffObj.eof() )
         {
            BSONObj obj ;
            rc = buffObj.nextObj( obj ) ;
            if ( rc )
            {
               PD_LOG( PDERROR, "Failed to get next object from buffer, rc: %d", rc ) ;
               goto error ;
            }
            
            sample.push_back( obj.getOwned() ) ;
         }
      }
      catch ( std::exception &e )
      {
         rc = SDB_SYS ;
         PD_LOG( PDERROR, "Exception occurred while taking sample: %s",
                 e.what() ) ;
         goto error ;
      }
      
   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _optDynamicSampler::takeSampleForFields( const CHAR *collectionName,
                                                  const vector<string> &fields,
                                                  UINT32 sampleSize,
                                                  vector<BSONObj> &sample )
   {
      INT32 rc = SDB_OK ;
      
      if ( !collectionName || fields.empty() || sampleSize == 0 )
      {
         rc = SDB_INVALIDARG ;
         PD_LOG( PDERROR, "Invalid arguments for takeSampleForFields" ) ;
         goto error ;
      }
      
      try
      {
         sample.clear() ;
         
         BSONObjBuilder projBuilder ;
         for ( UINT32 i = 0; i < fields.size(); i++ )
         {
            projBuilder.append( fields[i], 1 ) ;
         }
         BSONObj projection = projBuilder.obj() ;
         
         BSONObj query = BSON( "" << 1 ) ;
         BSONObj orderBy ;
         BSONObj hint ;
         
         pmdKRCB *krcb = pmdGetKRCB() ;
         SDB_RTNCB *rtnCB = krcb->getRTNCB() ;
         
         rtnContextBuf buffObj ;
         
         rc = rtnQuery( collectionName, projection, query, orderBy, hint,
                        0, 0, sampleSize, buffObj, NULL, NULL ) ;
         if ( rc )
         {
            PD_LOG( PDERROR, "Failed to execute query for sampling, rc: %d", rc ) ;
            goto error ;
         }
         
         while ( !buffObj.eof() )
         {
            BSONObj obj ;
            rc = buffObj.nextObj( obj ) ;
            if ( rc )
            {
               PD_LOG( PDERROR, "Failed to get next object from buffer, rc: %d", rc ) ;
               goto error ;
            }
            
            sample.push_back( obj.getOwned() ) ;
         }
      }
      catch ( std::exception &e )
      {
         rc = SDB_SYS ;
         PD_LOG( PDERROR, "Exception occurred while taking sample for fields: %s",
                 e.what() ) ;
         goto error ;
      }
      
   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _optDynamicSampler::buildHistograms( const CHAR *collectionName,
                                             const vector<string> &fields,
                                             UINT32 sampleSize,
                                             UINT32 numBuckets,
                                             optHistogramManager &histMgr )
   {
      INT32 rc = SDB_OK ;
      vector<BSONObj> sample ;
      
      rc = takeSampleForFields( collectionName, fields, sampleSize, sample ) ;
      if ( rc )
      {
         PD_LOG( PDERROR, "Failed to take sample for fields, rc: %d", rc ) ;
         goto error ;
      }
      
      rc = histMgr.buildFromSample( sample, fields, numBuckets ) ;
      if ( rc )
      {
         PD_LOG( PDERROR, "Failed to build histograms from sample, rc: %d", rc ) ;
         goto error ;
      }
      
   done:
      return rc ;
   error:
      goto done ;
   }

   static _optDynamicSampler s_optDynamicSampler ;

   optDynamicSampler* getOptDynamicSampler()
   {
      return &s_optDynamicSampler ;
   }
}
