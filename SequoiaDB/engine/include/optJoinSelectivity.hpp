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

   Source File Name = optJoinSelectivity.hpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#ifndef OPTJOINSELECTIVITY_HPP__
#define OPTJOINSELECTIVITY_HPP__

#include "core.hpp"
#include "oss.hpp"
#include "optStatUnit.hpp"

namespace engine
{
   /*
    * Join Selectivity Estimator
    * Provides advanced join selectivity estimation based on histograms
    * and statistics
    */
   class _optJoinSelectivity : public SDBObject
   {
   public:
      _optJoinSelectivity() ;
      ~_optJoinSelectivity() ;

      /*
       * Evaluate join selectivity between two fields
       * @param leftField [in] - The field name in the left collection
       * @param leftStat [in] - The statistics for the left collection
       * @param rightField [in] - The field name in the right collection
       * @param rightStat [in] - The statistics for the right collection
       * @return The estimated join selectivity (0.0 - 1.0)
       */
      double evalJoinSelectivity( const CHAR *leftField,
                                 const optCollectionStat *leftStat,
                                 const CHAR *rightField,
                                 const optCollectionStat *rightStat ) ;

   private:
      /*
       * Evaluate join selectivity using histograms
       * @param leftField [in] - The field name in the left collection
       * @param leftStat [in] - The statistics for the left collection
       * @param rightField [in] - The field name in the right collection
       * @param rightStat [in] - The statistics for the right collection
       * @return The estimated join selectivity (0.0 - 1.0), or -1.0 if histograms are not available
       */
      double _evalJoinSelectivityWithHistogram( const CHAR *leftField,
                                               const optCollectionStat *leftStat,
                                               const CHAR *rightField,
                                               const optCollectionStat *rightStat ) ;

      /*
       * Evaluate join selectivity using default estimation
       * @param leftField [in] - The field name in the left collection
       * @param leftStat [in] - The statistics for the left collection
       * @param rightField [in] - The field name in the right collection
       * @param rightStat [in] - The statistics for the right collection
       * @return The estimated join selectivity (0.0 - 1.0)
       */
      double _evalJoinSelectivityDefault( const CHAR *leftField,
                                         const optCollectionStat *leftStat,
                                         const CHAR *rightField,
                                         const optCollectionStat *rightStat ) ;
   } ;

   typedef _optJoinSelectivity optJoinSelectivity ;

   /*
    * Get the singleton instance of the join selectivity estimator
    * @return Pointer to the join selectivity estimator
    */
   optJoinSelectivity* getOptJoinSelectivity() ;
}

#endif // OPTJOINSELECTIVITY_HPP__
