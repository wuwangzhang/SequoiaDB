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

   Source File Name = qgmOptiHashJoin.hpp

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

#ifndef QGMOPTHASHJOIN_HPP__
#define QGMOPTHASHJOIN_HPP__

#include "qgmOptiTreeNode.hpp"
#include "qgmConditionNode.hpp"
#include "qgmOprUnit.hpp"
#include "qgmUtil.hpp"
#include "qgmParamTable.hpp"
#include <string>

namespace engine
{
   /*
    * Hash Join Operator
    * Implements hash join algorithm for equality joins
    * Builds hash table on smaller relation and probes with larger relation
    */
   class _qgmOptiHashJoin : public _qgmOptiTreeNode
   {
   private:
      INT32                _joinType ;
      qgmConditionNode     *_condition ;
      qgmOprUnitPtrVec     _oprUnits ;
      qgmOPFieldVec        _selector ;
      qgmOPFieldVec        _orderby ;
      QGM_VARLIST          _varList ;
      BOOLEAN              _hasMakeVar ;
      BOOLEAN              _hasPushSort ;
      qgmField             _uniqueNameR ;
      qgmField             _uniqueNameL ;
      QGM_HINS             _hints ;
      _qgmOptiTreeNode     **_outer ;
      _qgmOptiTreeNode     **_inner ;
      
      // Cost parameters for hash join
      double               _buildCPUCost ;
      double               _probeCPUCost ;
      double               _spillIOCost ;

   public:
      _qgmOptiHashJoin( INT32 type, _qgmPtrTable *table,
                        _qgmParamTable *param ) ;
      virtual ~_qgmOptiHashJoin() ;

   public:
      virtual INT32 init() ;
      virtual INT32 outputStream( qgmOpStream &stream ) ;
      virtual INT32 outputSort( qgmOPFieldVec &sortFields ) ;
      virtual string toString() const ;
      virtual BOOLEAN validateBeforeChange( QGM_OPTI_TYPE type ) const ;
      virtual INT32 handleHints( QGM_HINS &hints ) ;

      INT32 makeCondition() ;
      BOOLEAN needMakeCondition() const ;
      BOOLEAN canSwapInnerOuter() const ;
      INT32 swapInnerOuter() ;

      _qgmOptiTreeNode *outer() { return *_outer ; }
      _qgmOptiTreeNode *inner() { return *_inner ; }

   protected:
      virtual INT32 _pushOprUnit( qgmOprUnit *oprUnit, PUSH_FROM from ) ;
      virtual INT32 _removeOprUnit( qgmOprUnit *oprUnit ) ;
      virtual INT32 _updateChange( qgmOprUnit *oprUnit ) ;
      virtual INT32 _extend( _qgmOptiTreeNode *&exNode ) ;

   private:
      INT32 _makeOuterInner() ;
      INT32 _makeCondVar( qgmConditionNode *cond ) ;
      INT32 _createJoinUnit() ;
      INT32 _validate() ;
      INT32 _handleHints( _qgmOptiTreeNode *sub, const QGM_HINS &hint ) ;
   } ;

   typedef class _qgmOptiHashJoin qgmOptiHashJoin ;
}

#endif // QGMOPTHASHJOIN_HPP__
