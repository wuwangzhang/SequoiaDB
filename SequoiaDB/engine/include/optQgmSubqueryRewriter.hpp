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

   Source File Name = optQgmSubqueryRewriter.hpp

   Dependencies: N/A

   Restrictions: N/A

   Change Activity:
   defect Date        Who Description
   ====== =========== === ==============================================
          04/30/2025  DEV Initial Draft

   Last Changed =

*******************************************************************************/

#ifndef OPTQGMSUBQUERYREWRITER_HPP__
#define OPTQGMSUBQUERYREWRITER_HPP__

#include "core.hpp"
#include "oss.hpp"
#include "qgmOptiTree.hpp"
#include "optQgmStrategy.hpp"

namespace engine
{
   class _optQgmSubqueryRewriter ;
   typedef _optQgmSubqueryRewriter optQgmSubqueryRewriter ;

   optQgmSubqueryRewriter* getQgmSubqueryRewriter() ;

   class _optQgmSubqueryFlattenStrategy ;
   typedef _optQgmSubqueryFlattenStrategy optQgmSubqueryFlattenStrategy ;
}

#endif // OPTQGMSUBQUERYREWRITER_HPP__
