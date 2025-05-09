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

   Source File Name = pmdMaskingMgr.hpp

   Descriptive Name = Process MoDel Masking Manager Header

   When/how to use: this program may be used on binary and text-formatted
   versions of PMD component. This file contains the masking manager that
   manages data masking configurations.

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          09/05/2025  XJH Initial Draft

   Last Changed =

*******************************************************************************/
#ifndef PMD_MASKINGMGR_HPP__
#define PMD_MASKINGMGR_HPP__

#include "core.hpp"
#include "oss.hpp"
#include "ossUtil.hpp"
#include "ossMem.hpp"
#include "ossLatch.hpp"
#include "pd.hpp"
#include "../bson/bsonobj.h"
#include <vector>
#include <string>

namespace engine
{
   /*
    * Masking type define
    */
   enum PMD_MASKING_TYPE
   {
      PMD_MASKING_TYPE_NONE = 0,     // No masking
      PMD_MASKING_TYPE_ASTERISK = 1, // Display as ***
      PMD_MASKING_TYPE_REMOVE = 2,   // Remove field
      PMD_MASKING_TYPE_RANDOM = 3    // Display as random string
   } ;

   /*
    * Masking rule structure
    */
   struct _pmdMaskingRule : public SDBObject
   {
      std::string collection ;  // Collection name (format: cs.cl)
      std::string column ;      // Column/field name
      INT32 type ;              // Masking type (PMD_MASKING_TYPE)
      std::string whiteUser ;   // User exempt from masking

      _pmdMaskingRule()
      : type(PMD_MASKING_TYPE_NONE)
      {
      }

      _pmdMaskingRule(const std::string &coll, const std::string &col, 
                     INT32 maskType, const std::string &user)
      : collection(coll), column(col), type(maskType), whiteUser(user)
      {
      }
   } ;
   typedef _pmdMaskingRule pmdMaskingRule ;

   /*
    * Masking Manager
    */
   class _pmdMaskingMgr : public SDBObject
   {
   public:
      BOOLEAN isInitialized() const { return _initialized; }
   public:
      _pmdMaskingMgr() ;
      ~_pmdMaskingMgr() ;

      /*
       * Initialize masking manager
       * @param configFile - Path to the masking configuration file
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 init(const CHAR* configFile) ;

      /*
       * Reload masking configuration
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 reload() ;

      /*
       * Get masking rule for a field
       * @param collection - Collection name (format: cs.cl)
       * @param column - Column/field name
       * @param user - Current user name
       * @param maskType - [out] Masking type to apply
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 getMaskingRule(const CHAR* collection, const CHAR* column, 
                          const CHAR* user, INT32& maskType) ;

      /*
       * Apply masking to a field value
       * @param maskType - Masking type to apply
       * @param value - Original value
       * @param maskedValue - [out] Masked value
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 applyMasking(INT32 maskType, const CHAR* value, 
                        std::string& maskedValue) ;
                        
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

   private:
      /*
       * Parse masking configuration file
       * @param configFile - Path to the masking configuration file
       * @return - SDB_OK on success, otherwise error code
       */
      INT32 _parseConfig(const CHAR* configFile) ;

      /*
       * Generate random string of specified length
       * @param length - Length of random string
       * @return - Random string
       */
      std::string _generateRandomString(INT32 length) ;

   private:
      std::vector<pmdMaskingRule> _rules ;  // Masking rules
      ossSpinSLatch _latch ;                // Lock for thread safety
      BOOLEAN _initialized ;                // Initialization flag
   } ;
   typedef _pmdMaskingMgr pmdMaskingMgr ;

} // namespace engine

#endif // PMD_MASKINGMGR_HPP__
