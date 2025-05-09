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

   Source File Name = pmdMaskingProcessor.cpp

   Descriptive Name = Process MoDel Masking Processor Implementation

   When/how to use: this program may be used on binary and text-formatted
   versions of PMD component. This file contains implementation for pmdMaskingProcessor

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          09/05/2025  XJH Initial Draft

   Last Changed =

*******************************************************************************/

#include "pmdMaskingProcessor.hpp"
#include "pmd.hpp"
#include "pd.hpp"
#include "../bson/bson.h"

namespace engine
{
   _pmdMaskingProcessor::_pmdMaskingProcessor()
   {
   }

   _pmdMaskingProcessor::~_pmdMaskingProcessor()
   {
   }

   INT32 _pmdMaskingProcessor::processBSON(const CHAR* collection, 
                                          const CHAR* user,
                                          const bson::BSONObj& inputObj, 
                                          bson::BSONObj& outputObj)
   {
      INT32 rc = SDB_OK;
      _pmdMaskingMgr* pMaskingMgr = pmdGetKRCB()->getMaskingMgr();
      
      if (!pMaskingMgr || !pMaskingMgr->isInitialized() || !collection)
      {
         outputObj = inputObj.copy();
         goto done;
      }
      
      rc = pMaskingMgr->processBSON(collection, user, inputObj, outputObj);
      if (rc)
      {
         PD_LOG(PDWARNING, "Failed to process BSON for masking, rc: %d", rc);
         outputObj = inputObj.copy();
         rc = SDB_OK;
      }
      
   done:
      return rc;
   }

   INT32 _pmdMaskingProcessor::processBSONs(const CHAR* collection, 
                                           const CHAR* user,
                                           const std::vector<bson::BSONObj>& inputObjs,
                                           std::vector<bson::BSONObj>& outputObjs)
   {
      INT32 rc = SDB_OK;
      _pmdMaskingMgr* pMaskingMgr = pmdGetKRCB()->getMaskingMgr();
      
      outputObjs.clear();
      
      if (!pMaskingMgr || !pMaskingMgr->isInitialized() || !collection)
      {
         for (UINT32 i = 0; i < inputObjs.size(); ++i)
         {
            outputObjs.push_back(inputObjs[i].copy());
         }
         goto done;
      }
      
      for (UINT32 i = 0; i < inputObjs.size(); ++i)
      {
         bson::BSONObj maskedObj;
         rc = pMaskingMgr->processBSON(collection, user, inputObjs[i], maskedObj);
         if (rc)
         {
            PD_LOG(PDWARNING, "Failed to process BSON for masking, rc: %d", rc);
            outputObjs.push_back(inputObjs[i].copy());
            rc = SDB_OK;
         }
         else
         {
            outputObjs.push_back(maskedObj);
         }
      }
      
   done:
      return rc;
   }
   
   INT32 _pmdMaskingProcessor::processBSONBuffer(const CHAR* collection, 
                                               const CHAR* user,
                                               rtnContextBuf& buf)
   {
      INT32 rc = SDB_OK;
      PD_TRACE_ENTRY(SDB_PMDMASKINGPROCESSOR_PROCESSBSONBUFFER);
      
      _pmdMaskingMgr* pMaskingMgr = pmdGetKRCB()->getMaskingMgr();
      
      if (!pMaskingMgr || !pMaskingMgr->isInitialized() || !collection || 
          buf._recordNum <= 0 || buf._pBuff == NULL)
      {
         goto done;
      }
      
      try
      {
         rtnContextStoreBuf tempBuf;
         tempBuf.setCountMode(buf.isCountMode());
         
         const CHAR* pCurrent = buf._pBuff;
         INT32 recordsProcessed = 0;
         
         while (recordsProcessed < buf._recordNum)
         {
            bson::BSONObj obj(pCurrent);
            bson::BSONObj maskedObj;
            
            rc = pMaskingMgr->processBSON(collection, user, obj, maskedObj);
            if (rc)
            {
               PD_LOG(PDWARNING, "Failed to process BSON for masking, rc: %d", rc);
               tempBuf.append(obj);
               rc = SDB_OK;
            }
            else
            {
               tempBuf.append(maskedObj);
            }
            
            pCurrent += ossAlign4((UINT32)obj.objsize());
            recordsProcessed++;
         }
         
         if (tempBuf.numRecords() > 0)
         {
            rtnContextBuf maskedBuf;
            tempBuf.get(-1, maskedBuf);
            
            if (maskedBuf._buffSize <= buf._buffSize)
            {
               ossMemcpy(buf._pBuff, maskedBuf._pBuff, maskedBuf._buffSize);
            }
            else
            {
               PD_LOG(PDWARNING, "Masked buffer size (%d) exceeds original buffer size (%d)",
                     maskedBuf._buffSize, buf._buffSize);
            }
         }
      }
      catch (std::exception& e)
      {
         PD_LOG(PDERROR, "Exception during buffer masking: %s", e.what());
         rc = SDB_SYS;
         goto error;
      }
      
   done:
      PD_TRACE_EXITRC(SDB_PMDMASKINGPROCESSOR_PROCESSBSONBUFFER, rc);
      return rc;
   error:
      goto done;
   }
}
