/*******************************************************************************


   Copyright (C) 2011-2018 SequoiaDB Ltd.

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

   Source File Name = rawbson2json.c

   Descriptive Name = Raw BSON to JSON ( or CSV )

   When/how to use: this program may be used on binary and text-formatted
   versions of UTIL component.

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          09/14/2012  TW  Initial Draft

   Last Changed =

*******************************************************************************/
#include "rawbson2json.h"
#include "../client/jstobs.h"
#include "utilDecodeRawbson.hpp"
#include "pd.hpp"

// The caller pass the pointer for raw bson, and output buffer pointer and
// buffer length.
// the function returns true if bufferLen is good enough to hold data, otherwise
// it will return FALSE and the content in outputbuffer is not defined
BOOLEAN rawbson2json_ex ( const CHAR *bsonObj,
                      CHAR *pOutputBuffer,
                      INT32 bufferLen,
                      const CHAR *collection,
                      const CHAR *user )
{
   bson obj ;
   bson_init ( &obj ) ;
   bson_init_finished_data ( &obj, (char*)bsonObj ) ;
   
   if (collection && user)
   {
      CHAR *pBuffer = NULL ;
      INT32 bufSize = 0 ;
      BOOLEAN result = FALSE ;
      
      INT32 rc = SDB_OK ;
      rc = bsonCovertJson((CHAR*)bsonObj, &pBuffer, &bufSize, collection, user) ;
      if (SDB_OK == rc && pBuffer)
      {
         if (bufferLen > bufSize)
         {
            ossMemcpy(pOutputBuffer, pBuffer, bufSize) ;
            result = TRUE ;
         }
         
         SDB_OSS_FREE(pBuffer) ;
         return result ;
      }
   }
   
   return bsonToJson(pOutputBuffer, bufferLen, &obj, FALSE, FALSE) ;
}

BOOLEAN rawbson2json ( const CHAR *bsonObj,
                      CHAR *pOutputBuffer,
                      INT32 bufferLen )
{
   return rawbson2json_ex(bsonObj, pOutputBuffer, bufferLen, NULL, NULL);
}

BOOLEAN rawbson2csv_ex ( const CHAR *bsonObj,
                      CHAR *pOutputBuffer,
                      INT32 bufferLen,
                      const CHAR *collection,
                      const CHAR *user )
{
   bson obj ;
   bson_init ( &obj ) ;
   bson_init_finished_data ( &obj, (char*)bsonObj ) ;
   
   if (collection && user)
   {
      CHAR *pBuffer = NULL ;
      INT32 bufSize = 0 ;
      BOOLEAN result = FALSE ;
      
      INT32 rc = SDB_OK ;
      rc = bsonCovertJson((CHAR*)bsonObj, &pBuffer, &bufSize, collection, user) ;
      if (SDB_OK == rc && pBuffer)
      {
         if (bufferLen > bufSize)
         {
            ossMemcpy(pOutputBuffer, pBuffer, bufSize) ;
            result = TRUE ;
         }
         
         SDB_OSS_FREE(pBuffer) ;
         return result ;
      }
   }
   
   return bsonToJson(pOutputBuffer, bufferLen, &obj, TRUE, FALSE) ;
}

BOOLEAN rawbson2csv ( const CHAR *bsonObj,
                      CHAR *pOutputBuffer,
                      INT32 bufferLen )
{
   return rawbson2csv_ex(bsonObj, pOutputBuffer, bufferLen, NULL, NULL);
}
