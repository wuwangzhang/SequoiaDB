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

   Source File Name = optQgmSubqueryRewriter.hpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#ifndef OPTQGMSUBQUERYREWRITER_HPP__
#define OPTQGMSUBQUERYREWRITER_HPP__

#include "core.hpp"
#include "oss.hpp"
#include "qgmOptiTree.hpp"
#include "qgmOptiSelect.hpp"
#include "qgmOptiNLJoin.hpp"
#include "qgmConditionNode.hpp"
#include "optQgmStrategy.hpp"

namespace engine
{
   /*
    * Subquery Rewriter
    * Implements subquery flattening and decorrelation techniques
    * based on MySQL/MariaDB optimization strategies
    */
   class _optQgmSubqueryRewriter : public SDBObject
   {
   public:
      _optQgmSubqueryRewriter() ;
      ~_optQgmSubqueryRewriter() ;

      /*
       * Rewrite subqueries in the query tree
       * @param orgTree [in,out] - The query tree to rewrite
       * @return SDB_OK on success, otherwise error
       */
      INT32 rewrite( qgmOptTree &orgTree ) ;

   private:
      /*
       * Detect subqueries in the query tree
       * @param treeNode [in] - The node to check for subqueries
       * @param subqueries [out] - Vector to store detected subqueries
       * @return SDB_OK on success, otherwise error
       */
      INT32 _detectSubqueries( qgmOptiTreeNode *treeNode,
                              qgmOptiTreeNodePtrVec &subqueries ) ;

      /*
       * Check if a subquery can be flattened
       * @param subquery [in] - The subquery to check
       * @return TRUE if the subquery can be flattened, FALSE otherwise
       */
      BOOLEAN _canFlatten( qgmOptiTreeNode *subquery ) ;

      /*
       * Flatten a subquery into a join
       * @param orgTree [in,out] - The query tree
       * @param subquery [in] - The subquery to flatten
       * @param parent [in] - The parent node of the subquery
       * @return SDB_OK on success, otherwise error
       */
      INT32 _flattenSubquery( qgmOptTree &orgTree,
                             qgmOptiTreeNode *subquery,
                             qgmOptiTreeNode *parent ) ;

      /*
       * Create a join node from a subquery
       * @param orgTree [in,out] - The query tree
       * @param subquery [in] - The subquery to convert
       * @param parent [in] - The parent node of the subquery
       * @param joinNode [out] - The created join node
       * @return SDB_OK on success, otherwise error
       */
      INT32 _createJoinNode( qgmOptTree &orgTree,
                            qgmOptiTreeNode *subquery,
                            qgmOptiTreeNode *parent,
                            qgmOptiTreeNode *&joinNode ) ;

      /*
       * Create a join condition from a subquery
       * @param subquery [in] - The subquery to convert
       * @param parent [in] - The parent node of the subquery
       * @param condition [out] - The created condition
       * @return SDB_OK on success, otherwise error
       */
      INT32 _createJoinCondition( qgmOptiTreeNode *subquery,
                                 qgmOptiTreeNode *parent,
                                 qgmConditionNode *&condition ) ;

      /*
       * Check if a subquery is correlated
       * @param subquery [in] - The subquery to check
       * @param parent [in] - The parent node of the subquery
       * @return TRUE if the subquery is correlated, FALSE otherwise
       */
      BOOLEAN _isCorrelated( qgmOptiTreeNode *subquery,
                            qgmOptiTreeNode *parent ) ;

      /*
       * Find correlation predicates in a subquery
       * @param subquery [in] - The subquery to check
       * @param parent [in] - The parent node of the subquery
       * @param predicates [out] - Vector to store correlation predicates
       * @return SDB_OK on success, otherwise error
       */
      INT32 _findCorrelationPredicates( qgmOptiTreeNode *subquery,
                                       qgmOptiTreeNode *parent,
                                       qgmOprUnitPtrVec &predicates ) ;
   } ;

   typedef _optQgmSubqueryRewriter optQgmSubqueryRewriter ;

   /*
    * Get the singleton instance of the subquery rewriter
    * @return Pointer to the subquery rewriter
    */
   optQgmSubqueryRewriter* getQgmSubqueryRewriter() ;

   /*
    * Subquery Flattening Strategy
    * Strategy for flattening subqueries into joins
    */
   class _optQgmSubqueryFlattenStrategy : public _optQgmStrategyBase
   {
   public:
      _optQgmSubqueryFlattenStrategy() ;
      virtual ~_optQgmSubqueryFlattenStrategy() ;

      virtual INT32 calcResult( qgmOprUnit *oprUnit,
                               qgmOptiTreeNode *curNode,
                               qgmOptiTreeNode *subNode,
                               OPT_QGM_SS_RESULT &result ) ;

      virtual const CHAR* strategyName() const ;
   } ;

   typedef _optQgmSubqueryFlattenStrategy optQgmSubqueryFlattenStrategy ;
}

#endif // OPTQGMSUBQUERYREWRITER_HPP__
