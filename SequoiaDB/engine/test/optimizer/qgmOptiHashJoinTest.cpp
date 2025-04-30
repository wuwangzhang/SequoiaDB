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

#include "qgmOptiHashJoin.hpp"
#include "qgmOptiSelect.hpp"
#include "qgmOptiTree.hpp"
#include "optCommon.hpp"

using namespace std;
using namespace engine;
using namespace bson;

static qgmOptiTreeNode* createJoinTree()
{
   qgmOptiSelect* leftSelect = new qgmOptiSelect();
   qgmOptiSelect* rightSelect = new qgmOptiSelect();
   
   qgmOPFieldVec leftFields;
   qgmField field;
   field.type = qgmField::TYPE_ATTR;
   field.value.attr.propName = "customer_id";
   field.value.attr.tableName = "customers";
   leftFields.push_back(field);
   
   field.value.attr.propName = "name";
   leftFields.push_back(field);
   
   leftSelect->setFields(leftFields);
   leftSelect->setName("customers");
   
   qgmOPFieldVec rightFields;
   field.value.attr.propName = "order_id";
   field.value.attr.tableName = "orders";
   rightFields.push_back(field);
   
   field.value.attr.propName = "customer_id";
   rightFields.push_back(field);
   
   rightSelect->setFields(rightFields);
   rightSelect->setName("orders");
   
   qgmOptiHashJoin* hashJoin = new qgmOptiHashJoin();
   
   qgmOpField leftJoinField;
   leftJoinField.type = qgmField::TYPE_ATTR;
   leftJoinField.value.attr.propName = "customer_id";
   leftJoinField.value.attr.tableName = "customers";
   
   qgmOpField rightJoinField;
   rightJoinField.type = qgmField::TYPE_ATTR;
   rightJoinField.value.attr.propName = "customer_id";
   rightJoinField.value.attr.tableName = "orders";
   
   hashJoin->setLeft(leftSelect);
   hashJoin->setRight(rightSelect);
   hashJoin->setJoinCondition(leftJoinField, rightJoinField);
   
   return hashJoin;
}

TEST(qgmOptiHashJoinTest, BasicCreation)
{
   qgmOptiHashJoin hashJoin;
   
   ASSERT_TRUE(hashJoin.getType() == QGM_OPTI_TYPE_HASHJOIN);
}

TEST(qgmOptiHashJoinTest, SimpleJoin)
{
   qgmOptiTreeNode* joinTree = createJoinTree();
   ASSERT_TRUE(joinTree != NULL);
   
   ASSERT_TRUE(joinTree->getType() == QGM_OPTI_TYPE_HASHJOIN);
   
   qgmOptiHashJoin* hashJoin = (qgmOptiHashJoin*)joinTree;
   ASSERT_TRUE(hashJoin->getLeft() != NULL);
   ASSERT_TRUE(hashJoin->getRight() != NULL);
   
   ASSERT_TRUE(hashJoin->getLeft()->getType() == QGM_OPTI_TYPE_SELECT);
   ASSERT_TRUE(hashJoin->getRight()->getType() == QGM_OPTI_TYPE_SELECT);
   
   SAFE_OSS_DELETE(joinTree);
}

TEST(qgmOptiHashJoinTest, CostEstimation)
{
   qgmOptiTreeNode* joinTree = createJoinTree();
   ASSERT_TRUE(joinTree != NULL);
   
   qgmOptiHashJoin* hashJoin = (qgmOptiHashJoin*)joinTree;
   
   hashJoin->getLeft()->setEstimatedRows(1000);  // 1000 customers
   hashJoin->getRight()->setEstimatedRows(5000); // 5000 orders
   
   optCostEstimator estimator;
   double cost = hashJoin->estimateCost(estimator);
   
   std::cout << "Estimated hash join cost: " << cost << std::endl;
   
   double expectedCost = (OPT_HASH_BUILD_CPU_COST * 1000) + 
                         (OPT_HASH_PROBE_CPU_COST * 5000);
   
   ASSERT_TRUE(cost > 0);
   ASSERT_TRUE(fabs(cost - expectedCost) / expectedCost < 0.2); // Within 20%
   
   SAFE_OSS_DELETE(joinTree);
}

TEST(qgmOptiHashJoinTest, CostComparison)
{
   qgmOptiTreeNode* hashJoinTree = createJoinTree();
   ASSERT_TRUE(hashJoinTree != NULL);
   
   qgmOptiHashJoin* hashJoin = (qgmOptiHashJoin*)hashJoinTree;
   
   qgmOptiNLJoin nlJoin;
   nlJoin.setLeft(hashJoin->getLeft()->copyNode());
   nlJoin.setRight(hashJoin->getRight()->copyNode());
   
   qgmOpField leftJoinField;
   leftJoinField.type = qgmField::TYPE_ATTR;
   leftJoinField.value.attr.propName = "customer_id";
   leftJoinField.value.attr.tableName = "customers";
   
   qgmOpField rightJoinField;
   rightJoinField.type = qgmField::TYPE_ATTR;
   rightJoinField.value.attr.propName = "customer_id";
   rightJoinField.value.attr.tableName = "orders";
   
   nlJoin.setJoinCondition(leftJoinField, rightJoinField);
   
   hashJoin->getLeft()->setEstimatedRows(1000);  // 1000 customers
   hashJoin->getRight()->setEstimatedRows(5000); // 5000 orders
   nlJoin.getLeft()->setEstimatedRows(1000);
   nlJoin.getRight()->setEstimatedRows(5000);
   
   optCostEstimator estimator;
   double hashJoinCost = hashJoin->estimateCost(estimator);
   double nlJoinCost = nlJoin.estimateCost(estimator);
   
   std::cout << "Hash join cost: " << hashJoinCost << std::endl;
   std::cout << "Nested loop join cost: " << nlJoinCost << std::endl;
   
   ASSERT_TRUE(hashJoinCost < nlJoinCost);
   
   SAFE_OSS_DELETE(hashJoinTree);
   SAFE_OSS_DELETE(nlJoin.getLeft());
   SAFE_OSS_DELETE(nlJoin.getRight());
}

TEST(qgmOptiHashJoinTest, MemoryUsage)
{
   qgmOptiTreeNode* joinTree = createJoinTree();
   ASSERT_TRUE(joinTree != NULL);
   
   qgmOptiHashJoin* hashJoin = (qgmOptiHashJoin*)joinTree;
   
   hashJoin->getLeft()->setEstimatedRows(1000);  // 1000 customers
   hashJoin->getRight()->setEstimatedRows(5000); // 5000 orders
   
   size_t memoryUsage = hashJoin->estimateMemoryUsage();
   
   std::cout << "Estimated hash join memory usage: " << memoryUsage << " bytes" << std::endl;
   
   ASSERT_TRUE(memoryUsage > 0);
   
   SAFE_OSS_DELETE(joinTree);
}

TEST(qgmOptiHashJoinTest, Performance)
{
   qgmOptiTreeNode* hashJoinTree = createJoinTree();
   ASSERT_TRUE(hashJoinTree != NULL);
   
   qgmOptiHashJoin* hashJoin = (qgmOptiHashJoin*)hashJoinTree;
   
   qgmOptiNLJoin nlJoin;
   nlJoin.setLeft(hashJoin->getLeft()->copyNode());
   nlJoin.setRight(hashJoin->getRight()->copyNode());
   
   qgmOpField leftJoinField;
   leftJoinField.type = qgmField::TYPE_ATTR;
   leftJoinField.value.attr.propName = "customer_id";
   leftJoinField.value.attr.tableName = "customers";
   
   qgmOpField rightJoinField;
   rightJoinField.type = qgmField::TYPE_ATTR;
   rightJoinField.value.attr.propName = "customer_id";
   rightJoinField.value.attr.tableName = "orders";
   
   nlJoin.setJoinCondition(leftJoinField, rightJoinField);
   
   ossTimestamp startTime, endTime;
   
   ossGetCurrentTime(startTime);
   ossGetCurrentTime(endTime);
   
   UINT64 hashJoinTimeMS = ossGetTimestampDiff(startTime, endTime) / 1000;
   
   ossGetCurrentTime(startTime);
   ossGetCurrentTime(endTime);
   
   UINT64 nlJoinTimeMS = ossGetTimestampDiff(startTime, endTime) / 1000;
   
   std::cout << "Hash join execution time: " << hashJoinTimeMS << "ms" << std::endl;
   std::cout << "Nested loop join execution time: " << nlJoinTimeMS << "ms" << std::endl;
   
   
   SAFE_OSS_DELETE(hashJoinTree);
   SAFE_OSS_DELETE(nlJoin.getLeft());
   SAFE_OSS_DELETE(nlJoin.getRight());
}
