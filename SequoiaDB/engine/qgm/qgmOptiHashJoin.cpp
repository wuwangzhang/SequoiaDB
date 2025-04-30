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

   Source File Name = qgmOptiHashJoin.cpp

   Descriptive Name = Hash Join Optimization

   When/how to use: this program may be used on binary and text-formatted
   versions of PMD component. This file contains implementation for hash join
   optimization.

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#include "qgmOptiHashJoin.hpp"
#include "pd.hpp"
#include "qgmConditionNodeHelper.hpp"
#include "qgmOprUnit.hpp"
#include "qgmUtil.hpp"
#include "pmd.hpp"
#include "pmdCB.hpp"
#include "qgmHintDef.hpp"
#include "pdTrace.hpp"
#include "qgmTrace.hpp"
#include "qgmDef.hpp"

namespace engine
{
   _qgmOptiHashJoin::_qgmOptiHashJoin( INT32 type, _qgmPtrTable *table,
                                      _qgmParamTable *param )
   :_qgmOptiTreeNode( QGM_OPTI_TYPE_HASHJOIN, table, param ),
    _joinType( type ),
    _condition( NULL )
   {
      _outer = NULL ;
      _inner = NULL ;
      _hasMakeVar = FALSE ;
      _hasPushSort = FALSE ;
   }

   _qgmOptiHashJoin::~_qgmOptiHashJoin()
   {
      SAFE_OSS_DELETE( _condition ) ;
   }

   INT32 _qgmOptiHashJoin::_makeCondVar( qgmConditionNode * cond )
   {
      INT32 rc = SDB_OK ;
      BOOLEAN existed = FALSE ;
      varItem item ;

      if ( cond->type != SQL_GRAMMAR::EG )
      {
         PD_LOG_MSG( PDERROR, "Hash join only supports equality conditions" ) ;
         rc = SDB_SYS ;
         goto error ;
      }
      else if ( cond->left->type != SQL_GRAMMAR::DBATTR ||
                cond->right->type != SQL_GRAMMAR::DBATTR )
      {
         PD_LOG_MSG( PDERROR, "Hash join requires attributes on both sides" ) ;
         rc = SDB_SYS ;
         goto error ;
      }

      if ( cond->left->value.relegation() == outer()->getAlias( TRUE ) )
      {
         _qgmConditionNode *tmp = cond->left ;
         cond->left = cond->right ;
         cond->right = tmp ;
      }

      if ( cond->left->value.relegation() != inner()->getAlias( TRUE ) ||
           cond->right->value.relegation() != outer()->getAlias( TRUE ) )
      {
         PD_LOG_MSG( PDERROR, "Join condition[%s] invalid",
                     qgmConditionNodeHelper(cond).toJson().c_str() ) ;
         rc = SDB_INVALIDARG ;
         goto error ;
      }

      item._fieldName = cond->right->value ;
      item._varName = cond->right->value ;
      item._varName.relegation() = _uniqueNameR ;
      rc = _param->addVar( item._varName, cond->right->var, &existed ) ;
      if ( SDB_OK != rc )
      {
         PD_LOG( PDERROR, "Add var[%s, %s] failed, rc: %d",
                 item._fieldName.toString().c_str(),
                 item._varName.toString().c_str(), rc ) ;
         goto error ;
      }
      if ( !existed )
      {
         _varList.push_back( item ) ;
      }

      if ( _hints.empty() )
      {
         cond->right->type = SQL_GRAMMAR::SQLMAX + 1 ;
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   BOOLEAN _qgmOptiHashJoin::canSwapInnerOuter() const
   {
      if ( SQL_GRAMMAR::INNERJOIN != _joinType ||
           _hasMakeVar || _hasPushSort )
      {
         return FALSE ;
      }
      return TRUE ;
   }

   INT32 _qgmOptiHashJoin::swapInnerOuter()
   {
      if ( !canSwapInnerOuter() )
      {
         return SDB_SYS ;
      }

      qgmOptiTreeNode *tmp = *_outer ;
      *_outer = *_inner ;
      *_inner = tmp ;

      return SDB_OK ;
   }

   BOOLEAN _qgmOptiHashJoin::needMakeCondition() const
   {
      if ( _condition && !_hasMakeVar )
      {
         return TRUE ;
      }
      return FALSE ;
   }

   INT32 _qgmOptiHashJoin::makeCondition()
   {
      INT32 rc = SDB_OK ;
      qgmFilterUnit *condUnit = NULL ;

      if ( !needMakeCondition() )
      {
         goto done ; ;
      }

      rc = _makeCondVar( _condition ) ;
      if ( SDB_OK != rc )
      {
         goto error ;
      }

      rc = _createJoinUnit() ;
      if ( SDB_OK != rc )
      {
         goto error ;
      }

      if ( _hints.empty() )
      {
         condUnit = SDB_OSS_NEW qgmFilterUnit( QGM_OPTI_TYPE_FILTER ) ;
         if ( !condUnit )
         {
            rc = SDB_OOM ;
            goto error ;
         }
         condUnit->setCondition( _condition ) ;
         _condition = NULL ;
         condUnit->setDispatchAlias( inner()->getAlias( TRUE ) ) ;
         _oprUnits.push_back( condUnit ) ;
         condUnit = NULL ;
      }
      else
      {
         _varList.clear() ;
      }

      _hasMakeVar = TRUE ;

   done:
      return rc ;
   error:
      if ( condUnit )
      {
         SDB_OSS_DEL condUnit ;
      }
      goto done ;
   }

   INT32 _qgmOptiHashJoin::init()
   {
      INT32 rc = SDB_OK ;

      _table->getUniqueTableAlias( _uniqueNameR ) ;
      if ( !_hints.empty() )
      {
         _table->getUniqueTableAlias( _uniqueNameL) ;
      }

      rc = _makeOuterInner() ;
      if ( SDB_OK != rc )
      {
         goto error ;
      }

      if ( SQL_GRAMMAR::INNERJOIN != _joinType )
      {
         PD_LOG_MSG( PDERROR, "Hash join only supports inner join" ) ;
         rc = SDB_INVALIDARG ;
         goto error ;
      }

      if ( _condition )
      {
         rc = makeCondition() ;
         if ( SDB_OK != rc )
         {
            goto error ;
         }
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _qgmOptiHashJoin::_createJoinUnit()
   {
      INT32 rc = SDB_OK ;
      qgmOprUnit *joinUnit = NULL ;
      QGM_VARLIST::iterator it ;

      if ( _hints.empty() )
      {
         if ( 0 == _varList.size() )
         {
            goto done ;
         }

         joinUnit = SDB_OSS_NEW qgmOprUnit( QGM_OPTI_TYPE_HASHJOIN ) ;
         if ( !joinUnit )
         {
            rc = SDB_OOM ;
            goto error ;
         }

         it = _varList.begin() ;
         while ( it != _varList.end() )
         {
            qgmDbAttr &fieldName = (*it)._fieldName ;
            joinUnit->addOpField( qgmOpField( fieldName, SQL_GRAMMAR::DBATTR ),
                                 FALSE ) ;
            ++it ;
         }
         joinUnit->setDispatchAlias( outer()->getAlias(TRUE) ) ;
         _oprUnits.push_back( joinUnit ) ;
         joinUnit = NULL ;
      }
      else
      {
         SDB_ASSERT( NULL != _condition &&
                     NULL != _condition->left &&
                     NULL != _condition->right, "can not be NULL") ;
         SDB_ASSERT( SQL_GRAMMAR::DBATTR == _condition->left->type &&
                     SQL_GRAMMAR::DBATTR == _condition->right->type,
                     "impossible" ) ;

         joinUnit = SDB_OSS_NEW qgmOprUnit( QGM_OPTI_TYPE_HASHJOIN ) ;
         if ( !joinUnit )
         {
            rc = SDB_OOM ;
            goto error ;
         }
         joinUnit->setDispatchAlias( outer()->getAlias(TRUE) ) ;
         joinUnit->addOpField( qgmOpField(_condition->right->value,
                                         SQL_GRAMMAR::DBATTR),
                              FALSE) ;
         _oprUnits.push_back( joinUnit ) ;
         joinUnit = NULL ;

         joinUnit = SDB_OSS_NEW qgmOprUnit( QGM_OPTI_TYPE_HASHJOIN_CONDITION ) ;
         if ( !joinUnit )
         {
            rc = SDB_OOM ;
            goto error ;
         }
         joinUnit->setDispatchAlias( inner()->getAlias(TRUE) ) ;
         joinUnit->addOpField( qgmOpField(_condition->left->value,
                                         SQL_GRAMMAR::DBATTR),
                              FALSE) ;
         _oprUnits.push_back( joinUnit ) ;
         joinUnit = NULL ;
      }

   done:
      return rc ;
   error:
      if ( joinUnit )
      {
         SDB_OSS_DEL joinUnit ;
      }
      goto done ;
   }

   INT32 _qgmOptiHashJoin::_makeOuterInner()
   {
      if ( _children.size() != 2 )
      {
         return SDB_SYS ;
      }

      _outer = &_children[0] ;
      _inner = &_children[1] ;

      return SDB_OK ;
   }

   string _qgmOptiHashJoin::toString() const
   {
      stringstream ss ;
      ss << "{" << this->_qgmOptiTreeNode::toString() ;
      ss << ",JoinType:" << "hash" ;

      if ( NULL != _condition )
      {
         _qgmConditionNodeHelper condition( _condition ) ;
         ss << ",condition:" << condition.toJson() ;
      }

      ss << "}" ;
      return ss.str() ;
   }

   INT32 _qgmOptiHashJoin::outputSort( qgmOPFieldVec & sortFields )
   {
      return outer()->outputSort( sortFields ) ;
   }

   INT32 _qgmOptiHashJoin::outputStream( qgmOpStream &stream )
   {
      INT32 rc = SDB_OK ;
      SDB_ASSERT( 2 == _children.size(), "impossible" ) ;

      _qgmOptiTreeNode *left = _children.at( 0 ) ;
      _qgmOptiTreeNode *right = _children.at( 1 ) ;

      rc = left->outputStream( stream ) ;
      if ( SDB_OK != rc )
      {
         goto error ;
      }

      stream.next = SDB_OSS_NEW qgmOpStream() ;
      if ( NULL == stream.next )
      {
         PD_LOG( PDERROR, "failed to allocate mem." ) ;
         rc = SDB_OOM ;
         goto error ;
      }

      rc = right->outputStream( *(stream.next) ) ;
      if ( SDB_OK != rc )
      {
         goto error ;
      }
   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _qgmOptiHashJoin::_pushOprUnit( qgmOprUnit * oprUnit, PUSH_FROM from )
   {
      INT32 rc = SDB_OK ;
      qgmFilterUnit *filterUnit = NULL ;
      qgmFilterUnit *outerUnit = NULL ;
      qgmFilterUnit *innerUnit = NULL ;

      if ( needMakeCondition() )
      {
         rc = makeCondition() ;
         if ( SDB_OK != rc )
         {
            PD_LOG( PDERROR, "Node[%s] make condition failed, rc: %d",
                   toString().c_str(), rc ) ;
            goto error ;
         }
      }

      if ( QGM_OPTI_TYPE_SORT == oprUnit->getType() )
      {
         oprUnit->setDispatchAlias( outer()->getAlias( TRUE ) ) ;
         oprUnit->resetNodeID() ;
         _oprUnits.insert( _oprUnits.begin(), oprUnit ) ;
         _hasPushSort = TRUE ;
      }
      else if ( QGM_OPTI_TYPE_FILTER == oprUnit->getType() )
      {
         qgmConditionNodePtrVec conds ;
         qgmConditionNodePtrVec::iterator itCond ;
         BOOLEAN outerChange = FALSE ;
         BOOLEAN innerChange = FALSE ;

         outerUnit = SDB_OSS_NEW qgmFilterUnit( QGM_OPTI_TYPE_FILTER ) ;
         innerUnit = SDB_OSS_NEW qgmFilterUnit( QGM_OPTI_TYPE_FILTER ) ;
         if ( !outerUnit || !innerUnit )
         {
            rc = SDB_OOM ;
            goto error ;
         }
         outerUnit->setOptional( TRUE ) ;
         innerUnit->setOptional( TRUE ) ;

         filterUnit = (qgmFilterUnit*)oprUnit ;
         qgmOPFieldVec *fields = filterUnit->getFields() ;
         for ( UINT32 index = 0 ; index < fields->size() ; index++ )
         {
            qgmOpField &field = (*fields)[index] ;
            if ( field.value.relegation() == outer()->getAlias( TRUE ) )
            {
               outerUnit->addOpField( field ) ;
               outerChange = TRUE ;
            }
            else if ( field.value.relegation() == inner()->getAlias( TRUE ) )
            {
               innerUnit->addOpField( field ) ;
               innerChange = TRUE ;
            }
            else
            {
               PD_LOG_MSG( PDERROR,
                          "oprUnit[%s] field[%s] is not in outer[%s] or "
                          "inner[%s]", oprUnit->toString().c_str(),
                          field.toString().c_str(),
                          outer()->getAlias( TRUE ).toString().c_str(),
                          inner()->getAlias( TRUE ).toString().c_str() ) ;
               rc = SDB_INVALIDARG ;
               goto error ;
            }
         }

         conds = filterUnit->getConditions() ;
         itCond = conds.begin() ;
         while ( itCond != conds.end() )
         {
            qgmConditionNode *condNode = *itCond ;
            while ( condNode->left )
            {
               condNode = condNode->left ;
            }

            if ( condNode->type != SQL_GRAMMAR::DBATTR )
            {
               outerUnit->addCondition( *itCond ) ;
               innerUnit->addCondition( SDB_OSS_NEW qgmConditionNode( *itCond ) ) ;
               outerChange = TRUE ;
               innerChange = TRUE ;
            }
            else if ( condNode->value.relegation() == outer()->getAlias( TRUE ) )
            {
               outerUnit->addCondition( *itCond ) ;
               outerChange = TRUE ;
            }
            else if ( condNode->value.relegation() == inner()->getAlias( TRUE ) )
            {
               innerUnit->addCondition( *itCond ) ;
               innerChange = TRUE ;
            }
            else
            {
               PD_LOG_MSG( PDERROR,
                          "oprUnit[%s] condition attr[%s] is not in "
                          "outer[%s] or inner[%s]", oprUnit->toString().c_str(),
                          condNode->value.toString().c_str(),
                          outer()->getAlias( TRUE ).toString().c_str(),
                          inner()->getAlias( TRUE ).toString().c_str() ) ;
               rc = SDB_INVALIDARG ;
               goto error ;
            }

            ++itCond ;
         }

         if ( !outerChange && _varList.size() == 0 )
         {
            qgmOpField dummyField ;
            dummyField.value.relegation() = outer()->getAlias( TRUE ) ;
            _table->getUniqueFieldAlias( dummyField.value.attr() ) ;
            dummyField.type = SQL_GRAMMAR::DBATTR ;
            outerUnit->addOpField( dummyField ) ;
         }

         QGM_VARLIST::iterator itVar = _varList.begin() ;
         while ( itVar != _varList.end() )
         {
            if ( !isFromOne( (*itVar)._fieldName, *(outerUnit->getFields()),
                            FALSE ) &&
                ((*itVar)._fieldName.relegation() ==
                outer()->getAlias()) )
            {
               outerUnit->addOpField( qgmOpField( (*itVar)._fieldName,
                                                 SQL_GRAMMAR::DBATTR) ) ;
            }
            ++itVar ;
         }

         if ( !_hints.empty() )
         {
            SDB_ASSERT( _varList.empty(), "must be empty" ) ;
            _qgmConditionNodeHelper ctree( _condition ) ;
            qgmDbAttrPtrVec attrVec ;
            ctree.getAllAttr( attrVec ) ;
            qgmDbAttrPtrVec::const_iterator itr = attrVec.begin() ;
            for ( ; itr != attrVec.end(); itr++ )
            {
               if ( !isFromOne( **itr, *(outerUnit->getFields()),
                              FALSE ) &&
                   ((*itr)->relegation() ==
                   outer()->getAlias()) )
               {
                  outerUnit->addOpField( qgmOpField( **itr,
                                        SQL_GRAMMAR::DBATTR) ) ;
               }
               else if ( !isFromOne( **itr, *(innerUnit->getFields()),
                              FALSE ) &&
                         ((*itr)->relegation() ==
                         inner()->getAlias()))
               {
                  innerUnit->addOpField( qgmOpField(**itr,
                                       SQL_GRAMMAR::DBATTR) ) ;
               }
            }
         }

         outerUnit->setDispatchAlias( outer()->getAlias( TRUE ) ) ;
         _oprUnits.push_back( outerUnit ) ;
         outerUnit = NULL ;

         if ( !innerChange )
         {
            qgmOpField dummyField ;
            dummyField.value.relegation() = inner()->getAlias( TRUE ) ;
            _table->getUniqueFieldAlias( dummyField.value.attr() ) ;
            dummyField.type = SQL_GRAMMAR::DBATTR ;
            innerUnit->addOpField( dummyField ) ;
         }

         innerUnit->setDispatchAlias( inner()->getAlias( TRUE ) ) ;
         _oprUnits.push_back( innerUnit ) ;
         innerUnit = NULL ;

         filterUnit->emptyCondition() ;
         SDB_OSS_DEL filterUnit ;
      }
      else
      {
         rc = SDB_SYS ;
         goto error ;
      }

   done:
      return rc ;
   error:
      if ( outerUnit )
      {
         SDB_OSS_DEL outerUnit ;
      }
      if ( innerUnit )
      {
         SDB_OSS_DEL innerUnit ;
      }
      goto done ;
   }

   INT32 _qgmOptiHashJoin::_removeOprUnit( qgmOprUnit * oprUnit )
   {
      return SDB_OK ;
   }

   INT32 _qgmOptiHashJoin::_updateChange( qgmOprUnit * oprUnit )
   {
      INT32 rc = SDB_OK ;

      if ( QGM_OPTI_TYPE_HASHJOIN != oprUnit->getType() &&
           QGM_OPTI_TYPE_HASHJOIN_CONDITION != oprUnit->getType() )
      {
         goto done ;
      }

      if ( _hints.empty() )
      {
         if ( oprUnit->getFields()->size() != _varList.size() )
         {
            PD_LOG( PDERROR, "Node[%s] joinUnit[%s] field num is not with the"
                   "varList", toString().c_str(), oprUnit->toString().c_str() ) ;
            SDB_ASSERT( FALSE , "JoinUnit field num is not with varList" ) ;
            rc = SDB_SYS ;
            goto error ;
         }

         qgmOPFieldVec *fields = oprUnit->getFields() ;
         UINT32 count = _varList.size() ;
         UINT32 index = 0 ;
         while ( index < count )
         {
            _varList[index]._fieldName = (*fields)[index].value ;
            ++index ;
         }
      }
      else
      {
         qgmOPFieldVec *fields = oprUnit->getFields() ;
         SDB_ASSERT( 1 == fields->size(), "size must be one") ;
         if ( oprUnit->getDispatchAlias() == inner()->getAlias() )
         {
            _condition->left->value = fields->at( 0 ).value ;
         }
         else
         {
            _condition->right->value = fields->at( 0 ).value ;
         }
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _qgmOptiHashJoin::handleHints( QGM_HINS &hints )
   {
      INT32 rc = SDB_OK ;
      if ( SQL_GRAMMAR::INNERJOIN != _joinType)
      {
         goto done ;
      }
      {
      QGM_HINS::iterator itr = hints.begin() ;
      for ( ; itr != hints.end(); itr++ )
      {
         if ( 0 == ossStrncmp( itr->value.begin(),
                              QGM_HINT_HASHJOIN,
                              itr->value.size() ))
         {
            if ( NULL != _condition &&
                SQL_GRAMMAR::EG == _condition->type )
            {
               SDB_ASSERT( NULL != _condition->left &&
                          NULL != _condition->right, "impossible") ;
               if ( SQL_GRAMMAR::DBATTR == _condition->left->type &&
                   SQL_GRAMMAR::DBATTR == _condition->right->type )
               {
                  qgmHint hint = *itr ;
                  _hints.push_back( hint ) ;
               }
            }
         }
      }
      }

      if ( 2 != _children.size() )
      {
         goto done ;
      }

      if ( QGM_OPTI_TYPE_SCAN == _children[0]->getType() )
      {
         rc = _handleHints( _children[0], hints ) ;
         if ( SDB_OK != rc )
         {
            PD_LOG( PDERROR, "sub node failed to handle hint:%d", rc ) ;
            goto error ;
         }
      }

      if ( QGM_OPTI_TYPE_SCAN == _children[1]->getType() )
      {
         rc = _handleHints( _children[1], hints ) ;
         if ( SDB_OK != rc )
         {
            PD_LOG( PDERROR, "sub node failed to handle hint:%d", rc ) ;
            goto error ;
         }
      }
   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _qgmOptiHashJoin::_handleHints( _qgmOptiTreeNode *sub,
                                        const QGM_HINS &hint )
   {
      INT32 rc = SDB_OK ;
      QGM_HINS copy ;
      const qgmField &alias = sub->getAlias() ;
      QGM_HINS::const_iterator itr = hint.begin() ;
      for ( ; itr != hint.end(); itr++ )
      {
         if ( 0 == ossStrncmp( itr->value.begin(),
                              QGM_HINT_USEINDEX,
                              itr->value.size() ) &&
             2 == itr->param.size() )
         {
            const qgmField &tName = itr->param.begin()->value.attr() ;
            if ( alias == tName )
            {
               copy.push_back( *itr ) ;
               break ;
            }
         }
      }

      if ( copy.empty() )
      {
         goto done ;
      }

      rc = sub->handleHints( copy ) ;
      if ( SDB_OK != rc )
      {
         PD_LOG( PDERROR, "failed to handle hint in sub node:%d", rc ) ;
         goto error ;
      }
   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _qgmOptiHashJoin::_extend( _qgmOptiTreeNode *&exNode )
   {
      INT32 rc = SDB_OK ;
      SDB_ASSERT( 2 == _children.size(), "impossible" ) ;

      rc = _validate() ;
      if ( SDB_OK != rc )
      {
         goto error ;
      }

      exNode = this ;

   done:
      return rc ;
   error:
      goto done ;
   }

   INT32 _qgmOptiHashJoin::_validate()
   {
      SDB_ASSERT( 2 == _children.size(), "impossible" ) ;
      INT32 rc = SDB_OK ;
      _qgmOptiTreeNode *left = _children.at( 0 ) ;
      _qgmOptiTreeNode *right = _children.at( 1 ) ;

      if ( left->_alias == right->_alias )
      {
         PD_LOG_MSG( PDERROR, "same alias:%s",
                    left->_alias.toString().c_str() ) ;
         rc = SDB_INVALIDARG ;
         goto error ;
      }

      if ( NULL != _condition )
      {
         qgmOpStream lstream, rstream ;
         qgmDbAttrPtrVec conditionFields ;
         qgmDbAttrPtrVec::iterator citr ;
         _qgmConditionNodeHelper cTree( _condition ) ;

         cTree.getAllAttr( conditionFields ) ;
         rc = left->outputStream( lstream ) ;
         if ( SDB_OK != rc )
         {
            goto error ;
         }
         rc = right->outputStream( rstream ) ;
         if ( SDB_OK != rc )
         {
            goto error ;
         }

         for ( citr = conditionFields.begin()
              ; citr != conditionFields.end()
              ; citr++ )
         {
            if ( lstream.find( *(*citr) ) )
            {
               continue ;
            }
            else if ( !rstream.find( *(*citr) ) )
            {
               rc = SDB_INVALIDARG ;
               PD_LOG_MSG( PDERROR,
                          "condition field[%s] not found in sub output.",
                          (*citr)->toString().c_str() ) ;
               goto error ;
            }
            else
            {
               continue ;
            }
         }
      }
   done:
      return rc ;
   error:
      goto done ;
   }

   BOOLEAN _qgmOptiHashJoin::validateBeforeChange( QGM_OPTI_TYPE type ) const
   {
      return QGM_OPTI_TYPE_HASHJOIN == type ||
             QGM_OPTI_TYPE_HASHJOIN_CONDITION == type ;
   }
}
