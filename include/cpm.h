/* 
 * @brief Client Process Monitor interface
 *
 * @file cpm.h
 */
/* -----------------------------------------------------------------------------
 * Enduro/X Middleware Platform for Distributed Transaction Processing
 * Copyright (C) 2018, Mavimax, Ltd. All Rights Reserved.
 * This software is released under one of the following licenses:
 * GPL or Mavimax's license for commercial use.
 * -----------------------------------------------------------------------------
 * GPL license:
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * -----------------------------------------------------------------------------
 * A commercial use license is available from Mavimax, Ltd
 * contact@mavimax.com
 * -----------------------------------------------------------------------------
 */

#ifndef _CPM_H
#define	_CPM_H

#ifdef	__cplusplus
extern "C" {
#endif

/*---------------------------Includes-----------------------------------*/
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define CPM_CMD_PC          "pc" /* Print Clients(s) */
#define CPM_CMD_BC          "bc" /* Boot Client(s) */
#define CPM_CMD_SC          "sc" /* Stop Client(s) */
#define CPM_CMD_RC          "rc" /* Reload Client(s) (restart one by one) */
    
#define CPM_DEF_BUFFER_SZ       1024
    
#define CPM_OUTPUT_SIZE         256 /* Output buffer size */
    
#define CPM_TAG_LEN             128
#define CPM_SUBSECT_LEN         128
#define CPM_KEY_LEN             (CPM_TAG_LEN+1+CPM_SUBSECT_LEN) /* including FS in middle */
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/
    
#ifdef	__cplusplus
}
#endif

#endif	/* _CPM_H */

