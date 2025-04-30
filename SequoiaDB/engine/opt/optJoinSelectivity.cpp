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

   Source File Name = optJoinSelectivity.cpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#include "optJoinSelectivity.hpp"
#include "pd.hpp"
#include "optHistogram.hpp"
#include "optCommon.hpp"
#include "optStatUnit.hpp"
#include "pdTrace.hpp"
#include "optTrace.hpp"

namespace engine
{
   /*
    * _optJoinSelectivity implement
    */
   _optJoinSelectivity::_optJoinSelectivity()
   {
   }

   _optJoinSelectivity::~_optJoinSelectivity()
   {
   }

   double _optJoinSelectivity::evalJoinSelectivity( const CHAR *leftField,
                                                   const optCollectionStat *leftStat,
                                                   const CHAR *rightField,
                                                   const optCollectionStat *rightStat )
   {
      double selectivity = OPT_JOIN_DEFAULT_SELECTIVITY ;

      PD_TRACE_ENTRY( SDB__OPTJOINSEL_EVALJOINSEL ) ;

      if ( !leftField || !rightField || !leftStat || !rightStat )
      {
         goto done ;
      }

      selectivity = _evalJoinSelectivityWithHistogram( leftField, leftStat,
                                                      rightField, rightStat ) ;

      if ( selectivity < 0.0 )
      {
         selectivity = _evalJoinSelectivityDefault( leftField, leftStat,
                                                   rightField, rightStat ) ;
      }

      selectivity = max( selectivity, OPT_JOIN_MIN_SELECTIVITY ) ;
      selectivity = min( selectivity, OPT_JOIN_MAX_SELECTIVITY ) ;

   done:
      PD_TRACE_EXIT( SDB__OPTJOINSEL_EVALJOINSEL ) ;
      return selectivity ;
   }

   double _optJoinSelectivity::_evalJoinSelectivityWithHistogram( 
                                                   const CHAR *leftField,
                                                   const optCollectionStat *leftStat,
                                                   const CHAR *rightField,
                                                   const optCollectionStat *rightStat )
   {
      double selectivity = -1.0 ;

      PD_TRACE_ENTRY( SDB__OPTJOINSEL_EVALJOINSELWITHHIST ) ;

      try
      {
         optHistogramManager *leftHistMgr = NULL ;
         optHistogramManager *rightHistMgr = NULL ;

         if ( !leftHistMgr || !rightHistMgr )
         {
            goto done ;
         }

         selectivity = leftHistMgr->calculateJoinSelectivity( leftField, 
                                                             rightField, 
                                                             *rightHistMgr ) ;
      }
      catch ( std::exception &e )
      {
         PD_LOG( PDWARNING, "Exception occurred while evaluating join selectivity "
                            "with histogram: %s", e.what() ) ;
         selectivity = -1.0 ;
      }

   done:
      PD_TRACE_EXIT( SDB__OPTJOINSEL_EVALJOINSELWITHHIST ) ;
      return selectivity ;
   }

   double _optJoinSelectivity::_evalJoinSelectivityDefault( 
                                                   const CHAR *leftField,
                                                   const optCollectionStat *leftStat,
                                                   const CHAR *rightField,
                                                   const optCollectionStat *rightStat )
   {
      double selectivity = OPT_JOIN_DEFAULT_SELECTIVITY ;

      PD_TRACE_ENTRY( SDB__OPTJOINSEL_EVALJOINSELDEF ) ;

      const dmsIndexStat *leftFieldStat = leftStat->getFieldStat( leftField ) ;
      const dmsIndexStat *rightFieldStat = rightStat->getFieldStat( rightField ) ;

      if ( leftFieldStat && rightFieldStat )
      {
         UINT64 leftDistinct = leftFieldStat->getDistinctValues() ;
         UINT64 rightDistinct = rightFieldStat->getDistinctValues() ;

         if ( leftDistinct > 0 && rightDistinct > 0 )
         {
            selectivity = 1.0 / (double)max( leftDistinct, rightDistinct ) ;
         }
      }
      else
      {
         selectivity = OPT_JOIN_DEFAULT_SELECTIVITY ;
      }

      PD_TRACE_EXIT( SDB__OPTJOINSEL_EVALJOINSELDEF ) ;
      return selectivity ;
   }

   static _optJoinSelectivity s_optJoinSelectivity ;

   optJoinSelectivity* getOptJoinSelectivity()
   {
      return &s_optJoinSelectivity ;
   }
}
