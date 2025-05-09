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

   Source File Name = pmdMaskingProcessor.hpp

   Descriptive Name = Process MoDel Masking Processor Header

   When/how to use: this program may be used on binary and text-formatted
   versions of PMD component. This file contains the masking processor that
   applies masking rules to BSON objects.

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          09/05/2025  XJH Initial Draft

   Last Changed =

*******************************************************************************/
#ifndef PMD_MASKINGPROCESSOR_HPP__
#define PMD_MASKINGPROCESSOR_HPP__

#include "core.hpp"
#include "oss.hpp"
#include "ossUtil.hpp"
#include "ossMem.hpp"
#include "pmdMaskingMgr.hpp"
#include "../bson/bsonobj.h"

namespace engine
{
   /*
    * Masking Processor
    * Applies masking rules to BSON objects before they are returned to clients
    */
   class _pmdMaskingProcessor : public SDBObject
   {
   public:
      _pmdMaskingProcessor() ;
      ~_pmdMaskingProcessor() ;

      /*
       * Process a BSON object for masking
       * @param collection - Collection name (format: cs.cl)
       * @param user - Current user name
       * @param inputObj - Original BSON object
       * @param outputObj - [out] Masked BSON object
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 processBSON(const CHAR* collection, const CHAR* user,
                       const bson::BSONObj& inputObj, 
                       bson::BSONObj& outputObj) ;

      /*
       * Process a vector of BSON objects for masking
       * @param collection - Collection name (format: cs.cl)
       * @param user - Current user name
       * @param inputObjs - Original BSON objects
       * @param outputObjs - [out] Masked BSON objects
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 processBSONs(const CHAR* collection, const CHAR* user,
                        const std::vector<bson::BSONObj>& inputObjs,
                        std::vector<bson::BSONObj>& outputObjs) ;
                        
      /*
       * Process a context buffer for masking
       * @param collection - Collection name (format: cs.cl)
       * @param user - Current user name
       * @param buf - Context buffer to process
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 processBSONBuffer(const CHAR* collection, const CHAR* user,
                             rtnContextBuf& buf) ;
   } ;
   typedef _pmdMaskingProcessor pmdMaskingProcessor ;

} // namespace engine

#endif // PMD_MASKINGPROCESSOR_HPP__
