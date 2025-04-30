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

   Source File Name = optHistogram.hpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#ifndef OPTHISTOGRAM_HPP__
#define OPTHISTOGRAM_HPP__

#include "core.hpp"
#include "oss.hpp"
#include "ossUtil.hpp"
#include "ossMemPool.hpp"
#include "../bson/bson.h"
#include "dmsStatUnit.hpp"
#include "optCommon.hpp"

using namespace std ;
using namespace bson ;

namespace engine
{
   /*
    * Histogram bucket type
    */
   enum OPT_HISTOGRAM_BUCKET_TYPE
   {
      OPT_HISTOGRAM_BUCKET_SINGLETON = 0,  // Single value bucket
      OPT_HISTOGRAM_BUCKET_RANGE,          // Range of values
      OPT_HISTOGRAM_BUCKET_MAX
   } ;

   /*
    * Histogram bucket
    * Represents a single bucket in a histogram
    */
   class _optHistogramBucket : public SDBObject
   {
   public:
      _optHistogramBucket() ;
      _optHistogramBucket( const BSONElement &lowerBound, 
                           const BSONElement &upperBound,
                           UINT64 count,
                           OPT_HISTOGRAM_BUCKET_TYPE type ) ;
      ~_optHistogramBucket() ;

      /*
       * Get the lower bound of the bucket
       * @return The lower bound element
       */
      OSS_INLINE const BSONElement &getLowerBound() const
      {
         return _lowerBound ;
      }

      /*
       * Get the upper bound of the bucket
       * @return The upper bound element
       */
      OSS_INLINE const BSONElement &getUpperBound() const
      {
         return _upperBound ;
      }

      /*
       * Get the count of values in the bucket
       * @return The count
       */
      OSS_INLINE UINT64 getCount() const
      {
         return _count ;
      }

      /*
       * Get the bucket type
       * @return The bucket type
       */
      OSS_INLINE OPT_HISTOGRAM_BUCKET_TYPE getType() const
      {
         return _type ;
      }

      /*
       * Check if the bucket is a singleton
       * @return TRUE if the bucket is a singleton, FALSE otherwise
       */
      OSS_INLINE BOOLEAN isSingleton() const
      {
         return _type == OPT_HISTOGRAM_BUCKET_SINGLETON ;
      }

      /*
       * Check if the bucket is a range
       * @return TRUE if the bucket is a range, FALSE otherwise
       */
      OSS_INLINE BOOLEAN isRange() const
      {
         return _type == OPT_HISTOGRAM_BUCKET_RANGE ;
      }

      /*
       * Check if the value is in the bucket
       * @param value [in] - The value to check
       * @return TRUE if the value is in the bucket, FALSE otherwise
       */
      BOOLEAN contains( const BSONElement &value ) const ;

      /*
       * Calculate the selectivity of a value in the bucket
       * @param value [in] - The value to calculate selectivity for
       * @param totalCount [in] - The total count of values in the histogram
       * @return The selectivity (0.0 - 1.0)
       */
      double calculateSelectivity( const BSONElement &value, 
                                   UINT64 totalCount ) const ;

      /*
       * Calculate the selectivity of a range in the bucket
       * @param lower [in] - The lower bound of the range
       * @param lowerIncluded [in] - Whether the lower bound is included
       * @param upper [in] - The upper bound of the range
       * @param upperIncluded [in] - Whether the upper bound is included
       * @param totalCount [in] - The total count of values in the histogram
       * @return The selectivity (0.0 - 1.0)
       */
      double calculateRangeSelectivity( const BSONElement &lower, 
                                        BOOLEAN lowerIncluded,
                                        const BSONElement &upper, 
                                        BOOLEAN upperIncluded,
                                        UINT64 totalCount ) const ;

      /*
       * Convert the bucket to BSON
       * @return The BSON representation of the bucket
       */
      BSONObj toBSON() const ;

      /*
       * Create a bucket from BSON
       * @param obj [in] - The BSON representation of the bucket
       * @return The created bucket
       */
      static _optHistogramBucket *fromBSON( const BSONObj &obj ) ;

   private:
      BSONElement _lowerBound ;
      BSONElement _upperBound ;
      UINT64 _count ;
      OPT_HISTOGRAM_BUCKET_TYPE _type ;
      BSONObj _ownedLower ;
      BSONObj _ownedUpper ;
   } ;

   typedef _optHistogramBucket optHistogramBucket ;
   typedef vector<optHistogramBucket *> optHistogramBucketVec ;

   /*
    * Histogram
    * Represents a histogram for a field
    */
   class _optHistogram : public SDBObject
   {
   public:
      _optHistogram() ;
      _optHistogram( const CHAR *fieldName, UINT32 numBuckets ) ;
      ~_optHistogram() ;

      /*
       * Get the field name
       * @return The field name
       */
      OSS_INLINE const CHAR *getFieldName() const
      {
         return _fieldName.c_str() ;
      }

      /*
       * Get the number of buckets
       * @return The number of buckets
       */
      OSS_INLINE UINT32 getNumBuckets() const
      {
         return _buckets.size() ;
      }

      /*
       * Get the total count of values
       * @return The total count
       */
      OSS_INLINE UINT64 getTotalCount() const
      {
         return _totalCount ;
      }

      /*
       * Get the buckets
       * @return The buckets
       */
      OSS_INLINE const optHistogramBucketVec &getBuckets() const
      {
         return _buckets ;
      }

      /*
       * Add a bucket to the histogram
       * @param bucket [in] - The bucket to add
       * @return SDB_OK on success, otherwise error
       */
      INT32 addBucket( optHistogramBucket *bucket ) ;

      /*
       * Build the histogram from a sample
       * @param sample [in] - The sample data
       * @return SDB_OK on success, otherwise error
       */
      INT32 buildFromSample( const vector<BSONObj> &sample ) ;

      /*
       * Calculate the selectivity of a value
       * @param value [in] - The value to calculate selectivity for
       * @return The selectivity (0.0 - 1.0)
       */
      double calculateSelectivity( const BSONElement &value ) const ;

      /*
       * Calculate the selectivity of a range
       * @param lower [in] - The lower bound of the range
       * @param lowerIncluded [in] - Whether the lower bound is included
       * @param upper [in] - The upper bound of the range
       * @param upperIncluded [in] - Whether the upper bound is included
       * @return The selectivity (0.0 - 1.0)
       */
      double calculateRangeSelectivity( const BSONElement &lower, 
                                        BOOLEAN lowerIncluded,
                                        const BSONElement &upper, 
                                        BOOLEAN upperIncluded ) const ;

      /*
       * Calculate the join selectivity
       * @param otherHistogram [in] - The histogram of the other field
       * @return The join selectivity (0.0 - 1.0)
       */
      double calculateJoinSelectivity( const _optHistogram &otherHistogram ) const ;

      /*
       * Convert the histogram to BSON
       * @return The BSON representation of the histogram
       */
      BSONObj toBSON() const ;

      /*
       * Create a histogram from BSON
       * @param obj [in] - The BSON representation of the histogram
       * @return The created histogram
       */
      static _optHistogram *fromBSON( const BSONObj &obj ) ;

   private:
      string _fieldName ;
      optHistogramBucketVec _buckets ;
      UINT64 _totalCount ;
      UINT32 _maxBuckets ;
   } ;

   typedef _optHistogram optHistogram ;
   typedef ossPoolMap<string, optHistogram *> optHistogramMap ;

   /*
    * Histogram Manager
    * Manages histograms for a collection
    */
   class _optHistogramManager : public SDBObject
   {
   public:
      _optHistogramManager() ;
      ~_optHistogramManager() ;

      /*
       * Get a histogram for a field
       * @param fieldName [in] - The field name
       * @return The histogram, or NULL if not found
       */
      optHistogram *getHistogram( const CHAR *fieldName ) const ;

      /*
       * Add a histogram
       * @param histogram [in] - The histogram to add
       * @return SDB_OK on success, otherwise error
       */
      INT32 addHistogram( optHistogram *histogram ) ;

      /*
       * Build histograms from a sample
       * @param sample [in] - The sample data
       * @param fields [in] - The fields to build histograms for
       * @param numBuckets [in] - The number of buckets per histogram
       * @return SDB_OK on success, otherwise error
       */
      INT32 buildFromSample( const vector<BSONObj> &sample,
                             const vector<string> &fields,
                             UINT32 numBuckets ) ;

      /*
       * Calculate the join selectivity
       * @param leftField [in] - The left field name
       * @param rightField [in] - The right field name
       * @param rightHistMgr [in] - The histogram manager for the right collection
       * @return The join selectivity (0.0 - 1.0)
       */
      double calculateJoinSelectivity( const CHAR *leftField,
                                       const CHAR *rightField,
                                       const _optHistogramManager &rightHistMgr ) const ;

      /*
       * Convert the histogram manager to BSON
       * @return The BSON representation of the histogram manager
       */
      BSONObj toBSON() const ;

      /*
       * Create a histogram manager from BSON
       * @param obj [in] - The BSON representation of the histogram manager
       * @return The created histogram manager
       */
      static _optHistogramManager *fromBSON( const BSONObj &obj ) ;

   private:
      optHistogramMap _histograms ;
   } ;

   typedef _optHistogramManager optHistogramManager ;

   /*
    * Dynamic Sampler
    * Provides dynamic sampling for statistics collection
    */
   class _optDynamicSampler : public SDBObject
   {
   public:
      _optDynamicSampler() ;
      ~_optDynamicSampler() ;

      /*
       * Take a sample from a collection
       * @param collectionName [in] - The collection name
       * @param sampleSize [in] - The sample size
       * @param sample [out] - The sample data
       * @return SDB_OK on success, otherwise error
       */
      INT32 takeSample( const CHAR *collectionName,
                        UINT32 sampleSize,
                        vector<BSONObj> &sample ) ;

      /*
       * Take a sample from a collection for specific fields
       * @param collectionName [in] - The collection name
       * @param fields [in] - The fields to sample
       * @param sampleSize [in] - The sample size
       * @param sample [out] - The sample data
       * @return SDB_OK on success, otherwise error
       */
      INT32 takeSampleForFields( const CHAR *collectionName,
                                 const vector<string> &fields,
                                 UINT32 sampleSize,
                                 vector<BSONObj> &sample ) ;

      /*
       * Build histograms from a collection
       * @param collectionName [in] - The collection name
       * @param fields [in] - The fields to build histograms for
       * @param sampleSize [in] - The sample size
       * @param numBuckets [in] - The number of buckets per histogram
       * @param histMgr [out] - The histogram manager
       * @return SDB_OK on success, otherwise error
       */
      INT32 buildHistograms( const CHAR *collectionName,
                             const vector<string> &fields,
                             UINT32 sampleSize,
                             UINT32 numBuckets,
                             optHistogramManager &histMgr ) ;
   } ;

   typedef _optDynamicSampler optDynamicSampler ;

   /*
    * Get the singleton instance of the dynamic sampler
    * @return Pointer to the dynamic sampler
    */
   optDynamicSampler* getOptDynamicSampler() ;

}

#endif // OPTHISTOGRAM_HPP__
