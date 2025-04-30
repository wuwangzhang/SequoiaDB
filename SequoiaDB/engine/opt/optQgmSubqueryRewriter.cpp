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

   Source File Name = optQgmSubqueryRewriter.cpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#include "optQgmSubqueryRewriter.hpp"
#include "pd.hpp"
#include "qgmOptiSelect.hpp"
#include "qgmOptiNLJoin.hpp"
#include "qgmOptiHashJoin.hpp"
#include "qgmConditionNode.hpp"
#include "qgmUtil.hpp"

namespace engine
{
   _optQgmSubqueryRewriter::_optQgmSubqueryRewriter()
   {
   }

   _optQgmSubqueryRewriter::~_optQgmSubqueryRewriter()
   {
   }

   INT32 _optQgmSubqueryRewriter::rewrite( qgmOptTree &orgTree )
   {
      INT32 rc = SDB_OK ;
      qgmOptiTreeNodePtrVec subqueries ;
      qgmOptiTreeNode *root = orgTree.getRoot() ;

      if ( !root )
      {
         goto done ;
      }

      rc = _detectSubqueries( root, subqueries ) ;
      if ( SDB_OK != rc )
      {
         PD_LOG( PDERROR, "Failed to detect subqueries, rc: %d", rc ) ;
         goto error ;
      }

      for ( UINT32 i = 0; i < subqueries.size(); ++i )
      {
         qgmOptiTreeNode *subquery = subqueries[i] ;
         qgmOptiTreeNode *parent = subquery->getParent() ;

         if ( _canFlatten( subquery ) )
         {
            rc = _flattenSubquery( orgTree, subquery, parent ) ;
            if ( SDB_OK != rc )
            {
               PD_LOG( PDERROR, "Failed to flatten subquery, rc: %d", rc ) ;
               goto error ;
            }
         }
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _optQgmSubqueryRewriter::_detectSubqueries( qgmOptiTreeNode *treeNode,
                                                    qgmOptiTreeNodePtrVec &subqueries )
   {
      INT32 rc = SDB_OK ;
      qgmOptiTreeNodePtrVec children ;
      UINT32 childCount = 0 ;

      if ( !treeNode )
      {
         goto done ;
      }

      if ( QGM_OPTI_TYPE_SELECT == treeNode->getType() )
      {
         qgmOptiSelect *selectNode = (qgmOptiSelect *)treeNode ;
         
         if ( selectNode->getParent() && 
              ( selectNode->getParent()->getType() == QGM_OPTI_TYPE_FILTER ||
                selectNode->getParent()->getType() == QGM_OPTI_TYPE_JOIN ) )
         {
            subqueries.push_back( selectNode ) ;
         }
      }

      childCount = treeNode->getSubNodes( children ) ;
      for ( UINT32 i = 0; i < childCount; ++i )
      {
         rc = _detectSubqueries( children[i], subqueries ) ;
         if ( SDB_OK != rc )
         {
            PD_LOG( PDERROR, "Failed to detect subqueries in child node, rc: %d", rc ) ;
            goto error ;
         }
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   BOOLEAN _optQgmSubqueryRewriter::_canFlatten( qgmOptiTreeNode *subquery )
   {
      
      if ( QGM_OPTI_TYPE_SELECT != subquery->getType() )
      {
         return FALSE ;
      }

      qgmOptiSelect *selectNode = (qgmOptiSelect *)subquery ;
      
      qgmOprUnit *aggrUnit = selectNode->getOprUnitByType( QGM_OPTI_TYPE_AGGR ) ;
      if ( aggrUnit )
      {
         return FALSE ;
      }


      return TRUE ;
   }

   INT32 _optQgmSubqueryRewriter::_flattenSubquery( qgmOptTree &orgTree,
                                                   qgmOptiTreeNode *subquery,
                                                   qgmOptiTreeNode *parent )
   {
      INT32 rc = SDB_OK ;
      qgmOptiTreeNode *joinNode = NULL ;

      rc = _createJoinNode( orgTree, subquery, parent, joinNode ) ;
      if ( SDB_OK != rc )
      {
         PD_LOG( PDERROR, "Failed to create join node, rc: %d", rc ) ;
         goto error ;
      }

      rc = parent->updateSubNode( subquery, joinNode ) ;
      if ( SDB_OK != rc )
      {
         PD_LOG( PDERROR, "Failed to update subnode, rc: %d", rc ) ;
         goto error ;
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _optQgmSubqueryRewriter::_createJoinNode( qgmOptTree &orgTree,
                                                  qgmOptiTreeNode *subquery,
                                                  qgmOptiTreeNode *parent,
                                                  qgmOptiTreeNode *&joinNode )
   {
      INT32 rc = SDB_OK ;
      qgmConditionNode *condition = NULL ;
      BOOLEAN isCorrelated = FALSE ;

      isCorrelated = _isCorrelated( subquery, parent ) ;

      joinNode = orgTree.createNode( QGM_OPTI_TYPE_HASHJOIN ) ;
      if ( !joinNode )
      {
         rc = SDB_OOM ;
         PD_LOG( PDERROR, "Failed to create join node, rc: %d", rc ) ;
         goto error ;
      }

      rc = _createJoinCondition( subquery, parent, condition ) ;
      if ( SDB_OK != rc )
      {
         PD_LOG( PDERROR, "Failed to create join condition, rc: %d", rc ) ;
         goto error ;
      }


      if ( QGM_OPTI_TYPE_HASHJOIN == joinNode->getType() )
      {
         qgmOptiHashJoin *hashJoin = (qgmOptiHashJoin *)joinNode ;
         
         
         rc = hashJoin->init() ;
         if ( SDB_OK != rc )
         {
            PD_LOG( PDERROR, "Failed to initialize hash join, rc: %d", rc ) ;
            goto error ;
         }
      }

   done:
      return rc ;
   error:
      if ( joinNode )
      {
         SDB_OSS_DEL joinNode ;
         joinNode = NULL ;
      }
      goto done ;
   }

   INT32 _optQgmSubqueryRewriter::_createJoinCondition( qgmOptiTreeNode *subquery,
                                                       qgmOptiTreeNode *parent,
                                                       qgmConditionNode *&condition )
   {
      INT32 rc = SDB_OK ;
      qgmOprUnitPtrVec predicates ;

      rc = _findCorrelationPredicates( subquery, parent, predicates ) ;
      if ( SDB_OK != rc )
      {
         PD_LOG( PDERROR, "Failed to find correlation predicates, rc: %d", rc ) ;
         goto error ;
      }


      condition = NULL ;

   done:
      return rc ;
   error:
      goto done ;
   }

   BOOLEAN _optQgmSubqueryRewriter::_isCorrelated( qgmOptiTreeNode *subquery,
                                                  qgmOptiTreeNode *parent )
   {
      

      return TRUE ;
   }

   INT32 _optQgmSubqueryRewriter::_findCorrelationPredicates( qgmOptiTreeNode *subquery,
                                                             qgmOptiTreeNode *parent,
                                                             qgmOprUnitPtrVec &predicates )
   {
      INT32 rc = SDB_OK ;

      

      predicates.clear() ;

      return rc ;
   }

   optQgmSubqueryRewriter* getQgmSubqueryRewriter()
   {
      static optQgmSubqueryRewriter s_subqueryRewriter ;
      return &s_subqueryRewriter ;
   }

   _optQgmSubqueryFlattenStrategy::_optQgmSubqueryFlattenStrategy()
   {
   }

   _optQgmSubqueryFlattenStrategy::~_optQgmSubqueryFlattenStrategy()
   {
   }

   INT32 _optQgmSubqueryFlattenStrategy::calcResult( qgmOprUnit *oprUnit,
                                                    qgmOptiTreeNode *curNode,
                                                    qgmOptiTreeNode *subNode,
                                                    OPT_QGM_SS_RESULT &result )
   {
      INT32 rc = SDB_OK ;

      

      result = OPT_SS_REFUSE ;

      return rc ;
   }

   const CHAR* _optQgmSubqueryFlattenStrategy::strategyName() const
   {
      return "SubqueryFlatten" ;
   }
}
