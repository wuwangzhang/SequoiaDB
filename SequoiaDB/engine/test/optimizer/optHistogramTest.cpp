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

*******************************************************************************/

#include "ossTypes.hpp"
#include <gtest/gtest.h>
#include <iostream>

#include "optHistogram.hpp"
#include "optCommon.hpp"

using namespace std;
using namespace engine;
using namespace bson;

TEST(optHistogramTest, BasicCreation)
{
   optHistogram histogram("test_field", OPT_HISTOGRAM_DEFAULT_BUCKETS);
   
   ASSERT_TRUE(histogram.getBucketCount() == OPT_HISTOGRAM_DEFAULT_BUCKETS);
   ASSERT_TRUE(histogram.getFieldName() == "test_field");
   ASSERT_TRUE(histogram.getSampleCount() == 0);
}

TEST(optHistogramTest, AddValues)
{
   optHistogram histogram("test_field", 10);
   
   for (int i = 1; i <= 100; i++)
   {
      BSONObj value = BSON("value" << i);
      histogram.addValue(value["value"]);
   }
   
   ASSERT_TRUE(histogram.getSampleCount() == 100);
   
   ASSERT_TRUE(histogram.getBucketCount() == 10);
}

TEST(optHistogramTest, SelectivityEstimation)
{
   optHistogram histogram("test_field", 10);
   
   for (int i = 1; i <= 1000; i++)
   {
      BSONObj value = BSON("value" << i);
      histogram.addValue(value["value"]);
   }
   
   BSONObj pointValue = BSON("value" << 500);
   double pointSelectivity = histogram.estimateSelectivity(
      pointValue["value"], optHistogram::EQUALS);
   
   std::cout << "Point query selectivity: " << pointSelectivity << std::endl;
   ASSERT_TRUE(pointSelectivity > 0.0 && pointSelectivity < 0.02);
   
   BSONObj lowerValue = BSON("value" << 250);
   BSONObj upperValue = BSON("value" << 750);
   double rangeSelectivity = histogram.estimateRangeSelectivity(
      lowerValue["value"], upperValue["value"]);
   
   std::cout << "Range query selectivity: " << rangeSelectivity << std::endl;
   ASSERT_TRUE(rangeSelectivity > 0.45 && rangeSelectivity < 0.55);
}

TEST(optHistogramTest, SkewedDistribution)
{
   optHistogram histogram("test_field", 20);
   
   for (int i = 1; i <= 100; i++)
   {
      for (int j = 0; j < i; j++)
      {
         BSONObj value = BSON("value" << i);
         histogram.addValue(value["value"]);
      }
   }
   
   
   BSONObj lowerStart = BSON("value" << 1);
   BSONObj lowerEnd = BSON("value" << 20);
   double lowerSelectivity = histogram.estimateRangeSelectivity(
      lowerStart["value"], lowerEnd["value"]);
   
   BSONObj upperStart = BSON("value" << 80);
   BSONObj upperEnd = BSON("value" << 100);
   double upperSelectivity = histogram.estimateRangeSelectivity(
      upperStart["value"], upperEnd["value"]);
   
   std::cout << "Lower range selectivity: " << lowerSelectivity << std::endl;
   std::cout << "Upper range selectivity: " << upperSelectivity << std::endl;
   
   ASSERT_TRUE(upperSelectivity > lowerSelectivity);
}

TEST(optHistogramTest, MemoryUsage)
{
   optHistogram smallHist("test_field", 10);
   optHistogram largeHist("test_field", 100);
   
   for (int i = 1; i <= 1000; i++)
   {
      BSONObj value = BSON("value" << i);
      smallHist.addValue(value["value"]);
      largeHist.addValue(value["value"]);
   }
   
   size_t smallMemory = smallHist.getMemoryUsage();
   size_t largeMemory = largeHist.getMemoryUsage();
   
   std::cout << "Small histogram memory usage: " << smallMemory << " bytes" << std::endl;
   std::cout << "Large histogram memory usage: " << largeMemory << " bytes" << std::endl;
   
   ASSERT_TRUE(largeMemory > smallMemory);
   
   ASSERT_TRUE(largeMemory < 10240);
}

TEST(optHistogramTest, Serialization)
{
   optHistogram original("test_field", 10);
   
   for (int i = 1; i <= 100; i++)
   {
      BSONObj value = BSON("value" << i);
      original.addValue(value["value"]);
   }
   
   BSONObj serialized;
   INT32 rc = original.toBSON(serialized);
   ASSERT_TRUE(rc == SDB_OK);
   
   optHistogram deserialized;
   rc = deserialized.fromBSON(serialized);
   ASSERT_TRUE(rc == SDB_OK);
   
   ASSERT_TRUE(deserialized.getFieldName() == original.getFieldName());
   ASSERT_TRUE(deserialized.getBucketCount() == original.getBucketCount());
   ASSERT_TRUE(deserialized.getSampleCount() == original.getSampleCount());
   
   BSONObj testValue = BSON("value" << 50);
   double origSelectivity = original.estimateSelectivity(
      testValue["value"], optHistogram::EQUALS);
   double deserSelectivity = deserialized.estimateSelectivity(
      testValue["value"], optHistogram::EQUALS);
   
   ASSERT_TRUE(fabs(origSelectivity - deserSelectivity) < 0.0001);
}
