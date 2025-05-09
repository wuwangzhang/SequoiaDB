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
   versions of PMD component. This file contains implementation for pmdMaskingMgr

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
#include "ossIO.hpp"
#include "ossUtil.hpp"
#include "utilStr.hpp"
#include "../bson/bson.h"
#include "../bson/lib/nonce.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>

namespace engine
{
   _pmdMaskingMgr::_pmdMaskingMgr()
   : _initialized(FALSE)
   {
      std::srand(std::time(NULL));
   }

   _pmdMaskingMgr::~_pmdMaskingMgr()
   {
      _rules.clear();
   }

   INT32 _pmdMaskingMgr::init(const CHAR* configFile)
   {
      INT32 rc = SDB_OK;
      
      if (!configFile || '\0' == *configFile)
      {
         rc = SDB_INVALIDARG;
         PD_LOG(PDERROR, "Invalid masking config file path");
         goto error;
      }
      
      rc = _parseConfig(configFile);
      if (rc)
      {
         PD_LOG(PDERROR, "Failed to parse masking config file: %s, rc: %d",
                configFile, rc);
         goto error;
      }
      
      _initialized = TRUE;
      PD_LOG(PDEVENT, "Masking manager initialized with %d rules",
             (INT32)_rules.size());
      
   done:
      return rc;
   error:
      goto done;
   }

   INT32 _pmdMaskingMgr::reload()
   {
      return SDB_OK;
   }

   INT32 _pmdMaskingMgr::getMaskingRule(const CHAR* collection, 
                                       const CHAR* column,
                                       const CHAR* user, 
                                       INT32& maskType)
   {
      INT32 rc = SDB_OK;
      maskType = PMD_MASKING_TYPE_NONE;
      
      if (!collection || !column)
      {
         rc = SDB_INVALIDARG;
         goto error;
      }
      
      _latch.get();
      
      for (UINT32 i = 0; i < _rules.size(); ++i)
      {
         const pmdMaskingRule& rule = _rules[i];
         
         if (0 == ossStrcmp(rule.collection.c_str(), collection) &&
             0 == ossStrcmp(rule.column.c_str(), column))
         {
            if (user && rule.whiteUser.length() > 0 &&
                0 == ossStrcmp(rule.whiteUser.c_str(), user))
            {
               maskType = PMD_MASKING_TYPE_NONE;
            }
            else
            {
               maskType = rule.type;
            }
            break;
         }
      }
      
      _latch.release();
      
   done:
      return rc;
   error:
      goto done;
   }

   INT32 _pmdMaskingMgr::applyMasking(INT32 maskType, const CHAR* value,
                                     std::string& maskedValue)
   {
      INT32 rc = SDB_OK;
      
      if (!value)
      {
         maskedValue = "";
         goto done;
      }
      
      switch (maskType)
      {
         case PMD_MASKING_TYPE_ASTERISK:
            maskedValue = "***";
            break;
            
         case PMD_MASKING_TYPE_REMOVE:
            maskedValue = "";
            break;
            
         case PMD_MASKING_TYPE_RANDOM:
            maskedValue = _generateRandomString(ossStrlen(value));
            break;
            
         case PMD_MASKING_TYPE_NONE:
         default:
            maskedValue = value;
            break;
      }
      
   done:
      return rc;
   }
   
   INT32 _pmdMaskingMgr::processBSON(const CHAR* collection, const CHAR* user,
                                    const bson::BSONObj& inputObj, 
                                    bson::BSONObj& outputObj)
   {
      INT32 rc = SDB_OK;
      bson::BSONObjBuilder builder;
      
      if (!collection || !_initialized)
      {
         outputObj = inputObj.copy();
         goto done;
      }
      
      try
      {
         bson::BSONObjIterator it(inputObj);
         
         while (it.more())
         {
            bson::BSONElement elem = it.next();
            const CHAR* fieldName = elem.fieldName();
            INT32 maskType = PMD_MASKING_TYPE_NONE;
            
            rc = getMaskingRule(collection, fieldName, user, maskType);
            if (rc)
            {
               PD_LOG(PDWARNING, "Failed to get masking rule for field %s, rc: %d",
                      fieldName, rc);
               rc = SDB_OK;
               builder.append(elem);
               continue;
            }
            
            if (maskType == PMD_MASKING_TYPE_REMOVE)
            {
               continue;
            }
            else if (maskType == PMD_MASKING_TYPE_NONE)
            {
               builder.append(elem);
            }
            else
            {
               std::string maskedValue;
               std::string originalValue;
               
               if (elem.type() == bson::String)
               {
                  originalValue = elem.String();
               }
               else
               {
                  originalValue = elem.toString(false);
               }
               
               rc = applyMasking(maskType, originalValue.c_str(), maskedValue);
               if (rc)
               {
                  PD_LOG(PDWARNING, "Failed to apply masking for field %s, rc: %d",
                         fieldName, rc);
                  rc = SDB_OK;
                  builder.append(elem);
                  continue;
               }
               
               builder.append(fieldName, maskedValue);
            }
         }
         
         outputObj = builder.obj();
      }
      catch (std::exception& e)
      {
         PD_LOG(PDERROR, "Exception during BSON processing: %s", e.what());
         rc = SDB_SYS;
         goto error;
      }
      
   done:
      return rc;
   error:
      goto done;
   }

   INT32 _pmdMaskingMgr::_parseConfig(const CHAR* configFile)
   {
      INT32 rc = SDB_OK;
      std::ifstream file(configFile);
      std::string line;
      
      if (!file.is_open())
      {
         rc = SDB_IO;
         PD_LOG(PDERROR, "Failed to open masking config file: %s", configFile);
         goto error;
      }
      
      _latch.get();
      _rules.clear();
      
      while (std::getline(file, line))
      {
         if (line.empty() || '#' == line[0])
         {
            continue;
         }
         
         try
         {
            bson::BSONObj obj = bson::fromjson(line);
            
            std::string collection = obj.getStringField("Collection");
            std::string column = obj.getStringField("Column");
            INT32 type = obj.getIntField("Type");
            std::string whiteUser = obj.getStringField("WhiteUser");
            
            if (collection.empty() || column.empty() || type <= 0)
            {
               PD_LOG(PDWARNING, "Invalid masking rule: %s", line.c_str());
               continue;
            }
            
            _rules.push_back(pmdMaskingRule(collection, column, type, whiteUser));
            
            PD_LOG(PDDEBUG, "Added masking rule: collection=%s, column=%s, type=%d, whiteUser=%s",
                   collection.c_str(), column.c_str(), type, whiteUser.c_str());
         }
         catch (std::exception& e)
         {
            PD_LOG(PDWARNING, "Failed to parse masking rule: %s, error: %s",
                   line.c_str(), e.what());
            continue;
         }
      }
      
      _latch.release();
      file.close();
      
   done:
      return rc;
   error:
      if (file.is_open())
      {
         file.close();
      }
      goto done;
   }

   std::string _pmdMaskingMgr::_generateRandomString(INT32 length)
   {
      if (length <= 0)
      {
         return "";
      }
      
      static const CHAR charset[] = 
         "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
      
      std::string result;
      result.resize(length);
      
      bson::Security::Nonce nonce;
      
      for (INT32 i = 0; i < length; ++i)
      {
         result[i] = charset[nonce.nextInt32() % (sizeof(charset) - 1)];
      }
      
      return result;
   }
}
