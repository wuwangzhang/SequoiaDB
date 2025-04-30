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

#include "optQgmSubqueryRewriter.hpp"
#include "qgmOptiTree.hpp"
#include "qgmBuilder.hpp"
#include "qgmOptiSelect.hpp"
#include "qgmOptiNLJoin.hpp"

using namespace std;
using namespace engine;
using namespace bson;

static qgmOptiTreeNode* createSubqueryTree()
{
   qgmOptiSelect* outerSelect = new qgmOptiSelect();
   qgmOptiSelect* innerSelect = new qgmOptiSelect();
   
   qgmOPFieldVec outerFields;
   qgmField field;
   field.type = qgmField::TYPE_ATTR;
   field.value.attr.propName = "name";
   field.value.attr.branchName = "";
   field.value.attr.tableName = "customers";
   outerFields.push_back(field);
   outerSelect->setFields(outerFields);
   
   qgmOPFieldVec innerFields;
   field.value.attr.propName = "customer_id";
   field.value.attr.tableName = "orders";
   innerFields.push_back(field);
   innerSelect->setFields(innerFields);
   
   qgmOpField outerField;
   outerField.type = qgmField::TYPE_ATTR;
   outerField.value.attr.propName = "customer_id";
   outerField.value.attr.tableName = "customers";
   
   qgmOpField innerField;
   innerField.type = qgmField::TYPE_ATTR;
   innerField.value.attr.propName = "customer_id";
   innerField.value.attr.tableName = "orders";
   
   outerSelect->addSubQuery(innerSelect, outerField, innerField, SQL_GRAMMAR::IN);
   
   return outerSelect;
}

TEST(optQgmSubqueryRewriterTest, BasicFlattening)
{
   qgmOptiTreeNode* queryTree = createSubqueryTree();
   ASSERT_TRUE(queryTree != NULL);
   
   optQgmSubqueryRewriter* rewriter = getQgmSubqueryRewriter();
   ASSERT_TRUE(rewriter != NULL);
   
   INT32 rc = rewriter->rewrite(queryTree);
   ASSERT_TRUE(rc == SDB_OK);
   
   ASSERT_TRUE(queryTree->getType() == QGM_OPTI_TYPE_JOIN);
   
   SAFE_OSS_DELETE(queryTree);
}

TEST(optQgmSubqueryRewriterTest, NonFlattableSubquery)
{
   qgmOptiSelect* outerSelect = new qgmOptiSelect();
   qgmOptiSelect* innerSelect = new qgmOptiSelect();
   
   qgmOPFieldVec outerFields;
   qgmField field;
   field.type = qgmField::TYPE_ATTR;
   field.value.attr.propName = "name";
   field.value.attr.tableName = "customers";
   outerFields.push_back(field);
   outerSelect->setFields(outerFields);
   
   qgmOPFieldVec innerFields;
   field.type = qgmField::TYPE_AGGR;
   field.value.aggrType = AGGR_MAX;
   field.value.attr.propName = "order_total";
   field.value.attr.tableName = "orders";
   innerFields.push_back(field);
   innerSelect->setFields(innerFields);
   
   qgmOpField outerField;
   outerField.type = qgmField::TYPE_ATTR;
   outerField.value.attr.propName = "customer_id";
   outerField.value.attr.tableName = "customers";
   
   qgmOpField innerField;
   innerField.type = qgmField::TYPE_ATTR;
   innerField.value.attr.propName = "customer_id";
   innerField.value.attr.tableName = "orders";
   
   outerSelect->addSubQuery(innerSelect, outerField, innerField, SQL_GRAMMAR::IN);
   
   optQgmSubqueryRewriter* rewriter = getQgmSubqueryRewriter();
   ASSERT_TRUE(rewriter != NULL);
   
   INT32 rc = rewriter->rewrite(outerSelect);
   ASSERT_TRUE(rc == SDB_OK);
   
   ASSERT_TRUE(outerSelect->getType() == QGM_OPTI_TYPE_SELECT);
   
   SAFE_OSS_DELETE(outerSelect);
}

TEST(optQgmSubqueryRewriterTest, Performance)
{
   qgmOptiSelect* rootSelect = new qgmOptiSelect();
   
   ossTimestamp startTime, endTime;
   ossGetCurrentTime(startTime);
   
   optQgmSubqueryRewriter* rewriter = getQgmSubqueryRewriter();
   ASSERT_TRUE(rewriter != NULL);
   
   INT32 rc = rewriter->rewrite(rootSelect);
   ASSERT_TRUE(rc == SDB_OK);
   
   ossGetCurrentTime(endTime);
   
   UINT64 elapsedTimeMS = ossGetTimestampDiff(startTime, endTime) / 1000;
   
   std::cout << "Subquery flattening optimization time: " << elapsedTimeMS << "ms" << std::endl;
   ASSERT_TRUE(elapsedTimeMS < 100); // Arbitrary threshold for unit test
   
   SAFE_OSS_DELETE(rootSelect);
}
