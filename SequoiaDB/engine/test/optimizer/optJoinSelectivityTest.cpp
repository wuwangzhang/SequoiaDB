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

#include "optJoinSelectivity.hpp"
#include "optHistogram.hpp"
#include "optCommon.hpp"

using namespace std;
using namespace engine;
using namespace bson;

TEST(optJoinSelectivityTest, BasicEstimation)
{
   optJoinSelectivity estimator;
   
   double defaultSelectivity = estimator.estimateJoinSelectivity(
      "customers", "customer_id", "orders", "customer_id");
   
   std::cout << "Default join selectivity: " << defaultSelectivity << std::endl;
   ASSERT_TRUE(defaultSelectivity == OPT_JOIN_DEFAULT_SELECTIVITY);
}

TEST(optJoinSelectivityTest, HistogramBasedEstimation)
{
   optHistogram customerHist("customer_id", 10);
   optHistogram orderHist("customer_id", 10);
   
   for (int i = 1; i <= 1000; i++)
   {
      BSONObj value = BSON("value" << i);
      customerHist.addValue(value["value"]);
   }
   
   for (int i = 0; i < 5000; i++)
   {
      int customerId = (rand() % 1000) + 1;
      BSONObj value = BSON("value" << customerId);
      orderHist.addValue(value["value"]);
   }
   
   optJoinSelectivity estimator;
   
   estimator.registerHistogram("customers", "customer_id", &customerHist);
   estimator.registerHistogram("orders", "customer_id", &orderHist);
   
   double histSelectivity = estimator.estimateJoinSelectivity(
      "customers", "customer_id", "orders", "customer_id");
   
   std::cout << "Histogram-based join selectivity: " << histSelectivity << std::endl;
   
   ASSERT_TRUE(histSelectivity != OPT_JOIN_DEFAULT_SELECTIVITY);
   
   ASSERT_TRUE(histSelectivity >= OPT_JOIN_MIN_SELECTIVITY && 
               histSelectivity <= OPT_JOIN_MAX_SELECTIVITY);
}

TEST(optJoinSelectivityTest, SkewedDistribution)
{
   optHistogram customerHist("customer_id", 20);
   optHistogram orderHist("customer_id", 20);
   
   for (int i = 1; i <= 1000; i++)
   {
      BSONObj value = BSON("value" << i);
      customerHist.addValue(value["value"]);
   }
   
   for (int i = 0; i < 5000; i++)
   {
      int customerId;
      if (rand() % 100 < 80)
      {
         customerId = (rand() % 200) + 1;
      }
      else
      {
         customerId = (rand() % 800) + 201;
      }
      BSONObj value = BSON("value" << customerId);
      orderHist.addValue(value["value"]);
   }
   
   optJoinSelectivity estimator;
   
   estimator.registerHistogram("customers", "customer_id", &customerHist);
   estimator.registerHistogram("orders", "customer_id", &orderHist);
   
   double skewedSelectivity = estimator.estimateJoinSelectivity(
      "customers", "customer_id", "orders", "customer_id");
   
   std::cout << "Skewed distribution join selectivity: " << skewedSelectivity << std::endl;
   
   ASSERT_TRUE(skewedSelectivity < OPT_JOIN_DEFAULT_SELECTIVITY);
}

TEST(optJoinSelectivityTest, MultiColumnJoin)
{
   optJoinSelectivity estimator;
   
   double multiColSelectivity = estimator.estimateMultiColumnJoinSelectivity(
      "customers", "orders", 
      {"customer_id", "region"}, 
      {"customer_id", "ship_region"});
   
   std::cout << "Multi-column join selectivity: " << multiColSelectivity << std::endl;
   
   ASSERT_TRUE(multiColSelectivity < OPT_JOIN_DEFAULT_SELECTIVITY);
}

TEST(optJoinSelectivityTest, Performance)
{
   optJoinSelectivity estimator;
   
   optHistogram customerHist("customer_id", 100);
   optHistogram orderHist("customer_id", 100);
   
   for (int i = 1; i <= 10000; i++)
   {
      BSONObj value = BSON("value" << i);
      customerHist.addValue(value["value"]);
   }
   
   for (int i = 0; i < 50000; i++)
   {
      int customerId = (rand() % 10000) + 1;
      BSONObj value = BSON("value" << customerId);
      orderHist.addValue(value["value"]);
   }
   
   estimator.registerHistogram("customers", "customer_id", &customerHist);
   estimator.registerHistogram("orders", "customer_id", &orderHist);
   
   ossTimestamp startTime, endTime;
   ossGetCurrentTime(startTime);
   
   const int numEstimations = 1000;
   double totalSelectivity = 0.0;
   
   for (int i = 0; i < numEstimations; i++)
   {
      double selectivity = estimator.estimateJoinSelectivity(
         "customers", "customer_id", "orders", "customer_id");
      totalSelectivity += selectivity;
   }
   
   ossGetCurrentTime(endTime);
   
   UINT64 elapsedTimeUS = ossGetTimestampDiff(startTime, endTime);
   double avgTimeUS = (double)elapsedTimeUS / numEstimations;
   
   std::cout << "Average join selectivity estimation time: " << avgTimeUS << " microseconds" << std::endl;
   
   ASSERT_TRUE(avgTimeUS < 100.0);
}
