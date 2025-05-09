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

   Source File Name = pmdMaskingMgr.cpp

   Descriptive Name = Process MoDel Masking Manager Implementation

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

#include "pmdMaskingMgr.hpp"
#include "pd.hpp"
#include "pdTrace.hpp"
#include "pmdTrace.hpp"
#include "ossIO.hpp"
#include "ossUtil.hpp"
#include "../client/bson/bson.h"
#include "../client/jstobs.h"
#include <fstream>
#include <cstdlib>
#include <ctime>

namespace engine
{
   /*
    * Constructor
    */
   _pmdMaskingMgr::_pmdMaskingMgr()
   : _initialized(FALSE)
   {
      std::srand(std::time(NULL)) ;
   }

   /*
    * Destructor
    */
   _pmdMaskingMgr::~_pmdMaskingMgr()
   {
      _rules.clear() ;
   }

   /*
    * Initialize masking manager
    */
   INT32 _pmdMaskingMgr::init(const CHAR* configFile)
   {
      INT32 rc = SDB_OK ;

      if (_initialized)
      {
         goto done ;
      }

      if (!configFile)
      {
         rc = SDB_INVALIDARG ;
         PD_LOG(PDERROR, "Invalid masking config file") ;
         goto error ;
      }

      rc = _parseConfig(configFile) ;
      if (rc)
      {
         PD_LOG(PDERROR, "Failed to parse masking config file, rc=%d", rc) ;
         goto error ;
      }

      _initialized = TRUE ;

   done:
      return rc ;
   error:
      goto done ;
   }

   /*
    * Reload masking configuration
    */
   INT32 _pmdMaskingMgr::reload()
   {
      INT32 rc = SDB_OK ;
      std::string configFile ;

      if (configFile.empty())
      {
         rc = SDB_INVALIDARG ;
         PD_LOG(PDERROR, "No masking config file specified") ;
         goto error ;
      }

      _latch.get() ;
      _rules.clear() ;
      _latch.release() ;

      rc = _parseConfig(configFile.c_str()) ;
      if (rc)
      {
         PD_LOG(PDERROR, "Failed to reload masking config, rc=%d", rc) ;
         goto error ;
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   /*
    * Parse masking configuration file
    */
   INT32 _pmdMaskingMgr::_parseConfig(const CHAR* configFile)
   {
      INT32 rc = SDB_OK ;
      std::ifstream file(configFile) ;
      std::string line ;
      bson obj ;

      if (!file.is_open())
      {
         rc = SDB_IO ;
         PD_LOG(PDERROR, "Failed to open masking config file: %s, rc=%d",
                configFile, rc) ;
         goto error ;
      }

      while (std::getline(file, line))
      {
         if (line.empty() || line[0] == '#')
         {
            continue ;
         }

         bson_init(&obj) ;
         if (!json2bson2(line.c_str(), &obj))
         {
            PD_LOG(PDWARNING, "Invalid masking rule format: %s",
                   line.c_str()) ;
            bson_destroy(&obj) ;
            continue ;
         }

         bson_iterator it ;
         const CHAR* collection = NULL ;
         const CHAR* column = NULL ;
         INT32 type = PMD_MASKING_TYPE_NONE ;
         const CHAR* whiteUser = NULL ;

         if (bson_find(&it, &obj, "Collection") == BSON_STRING)
         {
            collection = bson_iterator_string(&it) ;
         }
         else
         {
            PD_LOG(PDWARNING, "Missing Collection field in masking rule: %s",
                   line.c_str()) ;
            bson_destroy(&obj) ;
            continue ;
         }

         if (bson_find(&it, &obj, "Column") == BSON_STRING)
         {
            column = bson_iterator_string(&it) ;
         }
         else
         {
            PD_LOG(PDWARNING, "Missing Column field in masking rule: %s",
                   line.c_str()) ;
            bson_destroy(&obj) ;
            continue ;
         }

         if (bson_find(&it, &obj, "Type") == BSON_INT)
         {
            type = bson_iterator_int(&it) ;
            if (type < PMD_MASKING_TYPE_NONE || type > PMD_MASKING_TYPE_RANDOM)
            {
               PD_LOG(PDWARNING, "Invalid Type value in masking rule: %s",
                      line.c_str()) ;
               bson_destroy(&obj) ;
               continue ;
            }
         }
         else
         {
            PD_LOG(PDWARNING, "Missing Type field in masking rule: %s",
                   line.c_str()) ;
            bson_destroy(&obj) ;
            continue ;
         }

         if (bson_find(&it, &obj, "WhiteUser") == BSON_STRING)
         {
            whiteUser = bson_iterator_string(&it) ;
         }

         _latch.get() ;
         _rules.push_back(pmdMaskingRule(collection, column, type,
                                         whiteUser ? whiteUser : "")) ;
         _latch.release() ;

         bson_destroy(&obj) ;
      }

      file.close() ;

      PD_LOG(PDEVENT, "Loaded %d masking rules from %s",
             (INT32)_rules.size(), configFile) ;

   done:
      return rc ;
   error:
      goto done ;
   }

   /*
    * Get masking rule for a field
    */
   INT32 _pmdMaskingMgr::getMaskingRule(const CHAR* collection, 
                                       const CHAR* column,
                                       const CHAR* user, 
                                       INT32& maskType)
   {
      INT32 rc = SDB_OK ;
      maskType = PMD_MASKING_TYPE_NONE ;

      if (!collection || !column)
      {
         rc = SDB_INVALIDARG ;
         goto error ;
      }

      _latch.get() ;
      for (UINT32 i = 0; i < _rules.size(); ++i)
      {
         const pmdMaskingRule& rule = _rules[i] ;
         
         if (rule.collection == collection && rule.column == column)
         {
            if (!rule.whiteUser.empty() && user && 
                rule.whiteUser == user)
            {
               continue ;
            }
            
            maskType = rule.type ;
            break ;
         }
      }
      _latch.release() ;

   done:
      return rc ;
   error:
      goto done ;
   }

   /*
    * Apply masking to a field value
    */
   INT32 _pmdMaskingMgr::applyMasking(INT32 maskType, const CHAR* value,
                                     std::string& maskedValue)
   {
      INT32 rc = SDB_OK ;

      if (!value)
      {
         rc = SDB_INVALIDARG ;
         goto error ;
      }

      switch (maskType)
      {
         case PMD_MASKING_TYPE_ASTERISK:
            maskedValue = "***" ;
            break ;
         
         case PMD_MASKING_TYPE_REMOVE:
            maskedValue = "" ;
            break ;
         
         case PMD_MASKING_TYPE_RANDOM:
            maskedValue = _generateRandomString(ossStrlen(value)) ;
            break ;
         
         case PMD_MASKING_TYPE_NONE:
         default:
            maskedValue = value ;
            break ;
      }

   done:
      return rc ;
   error:
      goto done ;
   }

   /*
    * Generate random string of specified length
    */
   std::string _pmdMaskingMgr::_generateRandomString(INT32 length)
   {
      static const CHAR charset[] = 
         "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" ;
      std::string result ;
      
      result.resize(length) ;
      
      for (INT32 i = 0; i < length; ++i)
      {
         result[i] = charset[std::rand() % (sizeof(charset) - 1)] ;
      }
      
      return result ;
   }

} // namespace engine
