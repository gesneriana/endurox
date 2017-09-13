/* 
** view<->json conversion lib
**
** @file view2exjson.c
** 
** -----------------------------------------------------------------------------
** Enduro/X Middleware Platform for Distributed Transaction Processing
** Copyright (C) 2015, Mavimax, Ltd. All Rights Reserved.
** This software is released under one of the following licenses:
** GPL or Mavimax's license for commercial use.
** -----------------------------------------------------------------------------
** GPL license:
** 
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; either version 2 of the License, or (at your option) any later
** version.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY
** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
** PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 59 Temple
** Place, Suite 330, Boston, MA 02111-1307 USA
**
** -----------------------------------------------------------------------------
** A commercial use license is available from Mavimax, Ltd
** contact@mavimax.com
** -----------------------------------------------------------------------------
*/#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>
#include <errno.h>

#include <userlog.h>

#include <ndrstandard.h>
#include <ndebug.h>

#include <exparson.h>
#include <ubf2exjson.h>
#include <ubf.h>
#include <atmi_int.h>
#include <typed_buf.h>

#include "tperror.h"


/*------------------------------Externs---------------------------------------*/
/*------------------------------Macros----------------------------------------*/
#define IS_INT(X) (BFLD_CHAR == X || BFLD_SHORT == X || BFLD_LONG == X || BFLD_INT == X)
#define IS_NUM(X) (BFLD_SHORT == X || BFLD_LONG == X || BFLD_FLOAT == X || BFLD_DOUBLE == X)
#define IS_BIN(X) (BFLD_CARRAY == X)

/* TODO: Fix atmi buffer size to match size of ATMI buffer size. */
#define CARR_BUFFSIZE       ATMI_MSG_MAX_SIZE
#define CARR_BUFFSIZE_B64   (4 * (CARR_BUFFSIZE) / 3)
/*------------------------------Enums-----------------------------------------*/
/*------------------------------Typedefs--------------------------------------*/
/*------------------------------Globals---------------------------------------*/
/*------------------------------Statics---------------------------------------*/
/*------------------------------Prototypes------------------------------------*/


exprivate long round_long( double r ) {
    return (r > 0.0) ? (r + 0.5) : (r - 0.5); 
}

/**
 * Convert JSON text buffer to UBF
 * @param p_ub - UBF buffer to fill data in
 * @param buffer - json text to parse
 * @return SUCCEED/FAIL
 */
expublic char* ndrx_tpjsontoview(char *view, char *buffer)
{
    int ret = EXSUCCEED;
    EXJSON_Value *root_value;
    EXJSON_Object *root_object;
    
    EXJSON_Value *view_value;
    EXJSON_Object *view_object;
    
    EXJSON_Value *val;
    EXJSON_Array *array_val;
    size_t i, cnt, j, arr_cnt;
    int type;
    char *name;
    char    *str_val;
    double d_val;
    int f_type;
    short   bool_val;
    char bin_buf[CARR_BUFFSIZE+1];
    char    *s_ptr;
    long vsize;
    int cnametyp;
    char *cstruct = NULL;

    NDRX_LOG(log_debug, "Parsing buffer: [%s]", buffer);

    root_value = exjson_parse_string_with_comments(buffer);
    type = exjson_value_get_type(root_value);
    NDRX_LOG(log_error, "Type is %d", type);

    if (exjson_value_get_type(root_value) != EXJSONObject)
    {
        NDRX_LOG(log_error, "Failed to parse root element");
        ndrx_TPset_error_fmt(TPEINVAL, "exjson: Failed to parse root element");
        return NULL;
    }
    root_object = exjson_value_get_object(root_value);

    cnt = exjson_object_get_count(root_object);
    NDRX_LOG(log_error, "cnt = %d", cnt);
    
    vsize = Bvsizeof(name);
    
    if (vsize < 0)
    {
        NDRX_LOG(log_error, "Failed to get view [%s] size: %s", 
                name, Bstrerror(Berror));
        
        ndrx_TPset_error_fmt(TPEINVAL, "Failed to get view [%s] size: %s", 
                name, Bstrerror(Berror));
        
        EXFAIL_OUT(ret);
    }
    
    name = (char *)exjson_object_get_name(root_object, i);
    
    NDRX_LOG(log_debug, "Allocating view [%s]: %ld", name, vsize);
    
    cstruct = tpalloc("VIEW", name, vsize);
    
    if (NULL==cstruct)
    {
        NDRX_LOG(log_error, "Failed to allocate view: %s", tpstrerror(tperrno));
        /* error must be set already! */
        EXFAIL_OUT(ret);
    }
    
    strcpy(view, name);
    
    view_object = exjson_object_get_object(root_object, name);
    
    if (NULL==view_object)
    {
        NDRX_LOG(log_error, "exjson: Failed to get view object");
        ndrx_TPset_error_msg(TPESYSTEM, "exjson: Failed to get view object");
        EXFAIL_OUT(ret);
    }

    for (i =0; i< cnt; i++)
    {
        name = (char *)exjson_object_get_name(view_object, i);

        NDRX_LOG(log_error, "came: [%s]", name);
        
        if (EXFAIL==Bvoccur(cstruct, view, name, NULL, NULL, NULL, &cnametyp))
        {
            NDRX_LOG(log_error, "Error getting field %s.%s infos: %s",
                    view, name, Bstrerror(Berror));
            
            if (BNOCNAME==Berror)
            {
                NDRX_LOG(log_debug, "%s.%s not found in view -> ignore",
                        view, name);
                continue;
            }
            else
            {
                ndrx_TPset_error_fmt(TPESYSTEM, "Failed to get %s.%s infos: %s",
                        view, name, Bstrerror(Berror));
                EXFAIL_OUT(ret);
            }
        }

        switch ((f_type=exjson_value_get_type(exjson_object_nget_value_n(view_object, i))))
        {
            case EXJSONString:
            {
                BFLDLEN str_len;
                s_ptr = str_val = (char *)exjson_object_get_string_n(view_object, i);
                NDRX_LOG(log_debug, "Str Value: [%s]", str_val);

                /* If it is carray - parse hex... */
                if (IS_BIN(cnametyp))
                {
                    size_t st_len;
                    NDRX_LOG(log_debug, "Field is binary..."
                            " convert from b64...");

                    if (NULL==atmi_base64_decode(str_val,
                            strlen(str_val),
                            &st_len,
                            bin_buf))
                    {
                        NDRX_LOG(log_debug, "Failed to "
                                "decode base64!");
                        
                        ndrx_TPset_error_fmt(TPEINVAL, "Failed to "
                                "decode base64: %s", name);
                        
                        EXFAIL_OUT(ret);
                    }
                    str_len = st_len;
                    s_ptr = bin_buf;
                    NDRX_LOG(log_error, "got binary len [%d]", str_len);
                }
                else
                {
                    str_len = strlen(s_ptr);
                }

                if (EXSUCCEED!=CBvchg(cstruct, view, name, 0, s_ptr, 
                        str_len, BFLD_CARRAY))
                {
                    NDRX_LOG(log_error, "Failed to set view field %s.%s: %s",
                            view, name, Bstrerror(Berror));
                    ndrx_TPset_error_fmt(TPESYSTEM, "Failed to set view field %s.%s: %s",
                            view, name, Bstrerror(Berror));
                    EXFAIL_OUT(ret);
                }
                
                break;
            }
            case EXJSONNumber:
            {
                long l;
                d_val = exjson_object_get_number_n(view_object, i);
                NDRX_LOG(log_debug, "Double Value: [%lf]", d_val);

                if (IS_INT(cnametyp))
                {
                    l = round_long(d_val);
                    if (EXSUCCEED!=CBvchg(cstruct, view, name, 0, 
                            (char *)&l, 0L, BFLD_LONG))
                    {
                        NDRX_LOG(log_error, "Failed to set [%s] to [%ld]!", 
                            name, l);
                        
                        ndrx_TPset_error_fmt(TPESYSTEM, "Failed to set [%s] to [%ld]!", 
                            name, l);
                        
                        EXFAIL_OUT(ret);
                    }
                }
                else if (EXSUCCEED!=CBvchg(cstruct, view, name, 0, 
                        (char *)&d_val, 0L, BFLD_DOUBLE))
                {
                    NDRX_LOG(log_error, "Failed to set [%s] to [%lf]: %s", 
                            name, d_val, Bstrerror(Berror));
                    
                    ndrx_TPset_error_fmt(TPESYSTEM, "Failed to set [%s] to [%lf]: %s", 
                            name, d_val, Bstrerror(Berror));
                    
                    EXFAIL_OUT(ret);
                }
            }
                    break;
            case EXJSONBoolean:
            {
                bool_val = (short)exjson_object_get_boolean_n(view_object, i);
                NDRX_LOG(log_debug, "Bool Value: [%hd]", bool_val);
                if (EXSUCCEED!=CBvchg(cstruct, view, name, 0, 
                        (char *)&bool_val, 0L, BFLD_SHORT))
                {
                    NDRX_LOG(log_error, "Failed to set [%s] to [%hd]: %s", 
                            name, bool_val, Bstrerror(Berror));
                    
                    ndrx_TPset_error_fmt(TPESYSTEM, "Failed to set [%s] to [%hd]: %s", 
                            name, bool_val, Bstrerror(Berror));
                    
                    EXFAIL_OUT(ret);
                }
            }
            break;
            /* Fielded buffer fields with more than one occurrance will go to array: 
             * Stuff here is almost identicial to above!
             */
            case EXJSONArray:
            {
                if (NULL==(array_val = exjson_value_get_array(
                        exjson_object_nget_value_n(view_object, i))))
                {
                    NDRX_LOG(log_error, "Failed to get array object!");
                    ndrx_TPset_error_fmt(TPESYSTEM, "Failed to get array object!");
                    EXFAIL_OUT(ret);
                }
                arr_cnt = exjson_array_get_count(array_val);

                for (j = 0; j<arr_cnt; j++ )
                {
                    switch (f_type = exjson_value_get_type(
                            exjson_array_get_value(array_val, j)))
                    {
                        case EXJSONString:
                        {
                            BFLDLEN str_len;
                            s_ptr = str_val = (char *)exjson_array_get_string(array_val, j);
                            NDRX_LOG(log_debug, 
                                        "Array j=%d, Str Value: [%s]", j, str_val);

                            /* If it is carray - parse hex... */
                            if (IS_BIN(cnametyp))
                            {
                                size_t st_len;
                                if (NULL==atmi_base64_decode(str_val,
                                        strlen(str_val),
                                        &st_len,
                                        bin_buf))
                                {
                                    NDRX_LOG(log_debug, "Failed to "
                                            "decode base64!");
                                    
                                    ndrx_TPset_error_fmt(TPEINVAL, "Failed to "
                                            "decode base64!");
                                    
                                    EXFAIL_OUT(ret);
                                }
                                str_len = st_len;
                                s_ptr = bin_buf;
                                NDRX_LOG(log_error, "got binary len [%d]", str_len);
                            }
                            else
                            {
                                str_len = strlen(s_ptr);
                            }

                            if (EXSUCCEED!=CBvchg(cstruct, view, name, j, 
                                    s_ptr, str_len, BFLD_CARRAY))
                            {
                                NDRX_LOG(log_error, "Failed to set [%s] to [%s]: %s", 
                                        name, str_val, Bstrerror(Berror));
                                ndrx_TPset_error_fmt(TPESYSTEM, "Failed to set [%s] "
                                        "to [%s]: %s", 
                                        name, str_val, Bstrerror(Berror));
                                
                                EXFAIL_OUT(ret);
                            }
                        }
                        break;
                        case EXJSONNumber:
                        {
                            long l;
                            d_val = exjson_array_get_number(array_val, j);
                            NDRX_LOG(log_debug, "Array j=%d, Double Value: [%lf]", j, d_val);

                            if (IS_INT(cnametyp))
                            {
                                l = round_long(d_val);
                                NDRX_LOG(log_debug, "Array j=%d, Long value: [%ld]", j, l);
                                if (EXSUCCEED!=CBvchg(cstruct, view, name, j, 
                                        (char *)&l, 0L, BFLD_LONG))
                                {
                                        NDRX_LOG(log_error, "Failed to set [%s] to [%ld]: %s", 
                                                name, l, Bstrerror(Berror));
                                        
                                        ndrx_TPset_error_fmt(TPESYSTEM, 
                                                "Failed to set [%s] to [%ld]: %s", 
                                                name, l, Bstrerror(Berror));
                                        
                                        EXFAIL_OUT(ret);
                                }
                            }
                            else if (EXSUCCEED!=CBvchg(cstruct, view, name, j, 
                                    (char *)&d_val, 0L, BFLD_DOUBLE))
                            {
                                NDRX_LOG(log_error, "Failed to set [%s] to [%lf]: %s", 
                                        name, d_val, Bstrerror(Berror));
                                
                                ndrx_TPset_error_fmt(TPESYSTEM,"Failed to set "
                                        "[%s] to [%lf]: %s", 
                                        name, d_val, Bstrerror(Berror));
                                
                                EXFAIL_OUT(ret);
                            }
                        }
                        break;
                        case EXJSONBoolean:
                        {
                            bool_val = (short)exjson_array_get_boolean(array_val, j);
                            NDRX_LOG(log_debug, "Array j=%d, Bool Value: [%hd]", j, bool_val);
                            if (EXSUCCEED!=CBvchg(cstruct, view, name, j, 
                                    (char *)&bool_val, 0L, BFLD_SHORT))
                            {
                                NDRX_LOG(log_error, "Failed to set [%s] to [%hd]: %s", 
                                        name, bool_val, Bstrerror(Berror));
                                
                                ndrx_TPset_error_fmt(TPESYSTEM,"Failed to set "
                                        "[%s] to [%hd]: %s", 
                                        name, bool_val, Bstrerror(Berror));
                                
                                EXFAIL_OUT(ret);
                            }
                        }
                        default:
                            NDRX_LOG(log_error, 
                                        "Unsupported array elem "
                                        "type: %d", f_type);                            
                        break;
                    }
                }

            }
            break;
            default:
            {
                NDRX_LOG(log_error, "Unsupported type: %d", f_type);
            }
            break;

        }
    }
    
out:
    /* cleanup code */
    exjson_value_free(root_value);

    if (EXSUCCEED!=ret && NULL!=cstruct)
    {
        tpfree(cstruct);
        cstruct = NULL;
    }

    
    return cstruct;
}

#if 0
TODO:
/**
 * Build json text from UBF buffer
 * @param p_ub  JSON buffer
 * @param buffer output json buffer
 * @param bufsize       output buffer size
 * @return SUCCEED/FAIL 
 */
expublic int ndrx_tpviewtojson(char *cstruct, char *view, char *buffer, int bufsize)
{
    int ret = EXSUCCEED;
    BFLDID fldid;
    int occs;
    int is_array;
    double d_val;
    char strval[CARR_BUFFSIZE+1]; 
    char b64_buf[CARR_BUFFSIZE_B64+1];
    int is_num;
    char *s_ptr;
    BFLDLEN flen;
    EXJSON_Value *root_value = exjson_value_init_object();
    EXJSON_Object *root_object = exjson_value_get_object(root_value);
    char *serialized_string = NULL;
    BFLDOCC oc;
    BFLDLEN fldlen;

    char *nm;
    EXJSON_Array * jarr;

    for (fldid = BFIRSTFLDID, oc = 0;
            1 == (ret = Bnext(p_ub, &fldid, &oc, NULL, &fldlen));)
    {
        nm = Bfname(fldid);
        NDRX_LOG(log_debug, "Field: [%s] occ %d", nm, oc);
        if (0==oc)
        {
            occs = Boccur(p_ub, fldid);

            if (occs>1)
            {
                /* create array */
                is_array = EXTRUE;
                if (NULL==(jarr = exjson_array_init()))
                {
                        NDRX_LOG(log_error, "Failed to initialise array!");
                        EXFAIL_OUT(ret);
                }
                /* add array to document... */
                if (EXJSONSuccess!=exjson_object_set_array(root_object, nm, jarr))
                {
                        NDRX_LOG(log_error, "Failed to add Array to root object!!");
                        EXFAIL_OUT(ret);
                }
            }
            else
            {
                is_array = EXFALSE;
            }
        }
        else
        {
            is_array = EXTRUE;
        }

        if (IS_NUM(fldid))
        {
            if (EXSUCCEED!=CBget(p_ub, fldid, oc, (char *)&d_val, 0L, BFLD_DOUBLE))
            {
                NDRX_LOG(log_error, "Failed to get (double): %ld/%d: %s",
                                                fldid, oc, Bstrerror(Berror));
                
                ndrx_TPset_error_fmt(TPESYSTEM, "Failed to get (double): %ld/%d: %s",
                                                fldid, oc, Bstrerror(Berror));
                EXFAIL_OUT(ret);
            }
            is_num = EXTRUE;
            NDRX_LOG(log_debug, "Numeric value: %lf", d_val);
        }
        else
        {
            is_num = EXFALSE;
            flen = sizeof(strval);
            if (EXSUCCEED!=CBget(p_ub, fldid, oc, strval, &flen, BFLD_CARRAY))
            {
                NDRX_LOG(log_error, "Failed to get (string): %ld/%d: %s",
                                        fldid, oc, Bstrerror(Berror));
                
                ndrx_TPset_error_fmt(TPESYSTEM, "Failed to get (string): %ld/%d: %s",
                                        fldid, oc, Bstrerror(Berror));
                
                EXFAIL_OUT(ret);
            }

            /* If it is carray, then convert to hex... */
            if (IS_BIN(fldid))
            {
                size_t outlen;
                NDRX_LOG(log_debug, "Field is binary... convert to b64");

                if (NULL==atmi_base64_encode((unsigned char *)strval, flen, 
                            &outlen, b64_buf))
                {
                    NDRX_LOG(log_error, "Failed to convert to b64!");
                    
                    ndrx_TPset_error_fmt(TPESYSTEM, "Failed to convert to b64!");
                    
                    EXFAIL_OUT(ret);
                }
                b64_buf[outlen] = EXEOS;
                s_ptr = b64_buf;

            }
            else
            {
                strval[flen] = EXEOS;
                s_ptr = strval;
            }

            NDRX_LOG(log_debug, "String value: [%s]", s_ptr);
        }

        if (is_array)
        {
                /* Add array element 
                exjson_object_set_value */

                /* Add normal element */
                if (is_num)
                {
                    if (EXJSONSuccess!=exjson_array_append_number(jarr, d_val))
                    {
                        NDRX_LOG(log_error, "Failed to set array elem to [%lf]!", 
                                d_val);
                        
                        ndrx_TPset_error_fmt(TPESYSTEM, "exjson: Failed to set array "
                                "elem to [%lf]!", d_val);
                        
                        EXFAIL_OUT(ret);
                    }
                }
                else
                {
                    if (EXJSONSuccess!=exjson_array_append_string(jarr, s_ptr))
                    {
                        NDRX_LOG(log_error, "Failed to set array elem to [%s]!", 
                                s_ptr);
                        
                        ndrx_TPset_error_fmt(TPESYSTEM, "exjson: Failed to set array "
                                "elem to [%s]!", s_ptr);
                        
                        EXFAIL_OUT(ret);
                    }
                }

        }
        else
        {
            /* Add normal element */
            if (is_num)
            {
                if (EXJSONSuccess!=exjson_object_set_number(root_object, nm, d_val))
                {
                    NDRX_LOG(log_error, "Failed to set [%s] value to [%lf]!",
                                        nm, d_val);
                    
                    ndrx_TPset_error_fmt(TPESYSTEM, "exjson: Failed to set [%s] "
                            "value to [%lf]!", nm, d_val);
                    
                    EXFAIL_OUT(ret);
                }
            }
            else
            {
                if (EXJSONSuccess!=exjson_object_set_string(root_object, nm, s_ptr))
                {
                    NDRX_LOG(log_error, "Failed to set [%s] value to [%s]!",
                                    nm, s_ptr);
                    
                    ndrx_TPset_error_fmt(TPESYSTEM, "exjson: Failed to set [%s] "
                            "value to [%s]!", nm, s_ptr);
                    
                    EXFAIL_OUT(ret);
                }
            }
        }
    }

    serialized_string = exjson_serialize_to_string(root_value);

    if (strlen(serialized_string) <= bufsize )
    {
        strncpy(buffer, serialized_string, bufsize-1);
        
        serialized_string[bufsize-1] = EXEOS;
        
        NDRX_LOG(log_debug, "Got JSON: [%s]", buffer);
    }
    else
    {
        NDRX_LOG(log_error, "Buffer too short: Got json size: [%d] buffer size: [%d]", 
                strlen(serialized_string), bufsize);
        
        ndrx_TPset_error_fmt(TPEOS, "Buffer too short: Got json size: "
                "[%d] buffer size: [%d]",  strlen(serialized_string), bufsize);
        
        EXFAIL_OUT(ret);
    }

out:

    if (NULL!=serialized_string)
    {
        exjson_free_serialized_string(serialized_string);
    }

    if (NULL!=root_value)
    {
        exjson_value_free(root_value);
    }
    return ret;
}

/**
 * auto-buffer convert func. json->ubf
 * @param buffer
 * @return 
 */
expublic int typed_xcvt_json2ubf(buffer_obj_t **buffer)
{
    int ret = EXSUCCEED;
    buffer_obj_t *tmp_b;
    /* Allocate the max UBF buffer */
    UBFH * tmp = NULL;
    UBFH * newbuf_out = NULL; /* real output buffer */

    if (NULL==(tmp = (UBFH *)tpalloc("UBF", NULL, ATMI_MSG_MAX_SIZE)))
    {
        NDRX_LOG(log_error, "failed to convert JSON->UBF. UBF buffer alloc fail!");
        EXFAIL_OUT(ret);
    }

    /* Do the convert */
    ndrx_TPunset_error();
    if (EXSUCCEED!=ndrx_tpjsontoubf(tmp, (*buffer)->buf))
    {
        tpfree((char *)tmp);
        NDRX_LOG(log_error, "Failed to convert JSON->UBF: %s", tpstrerror(tperrno));
        EXFAIL_OUT(ret);
    }

    /* Shrink the buffer (by reallocating) new! 
     * we will do the shrink because, msg Q might not have settings set for
     * max buffer size...
     */
    if (NULL==(newbuf_out = (UBFH *)tpalloc("UBF", NULL, Bused(tmp))))
    {
        tpfree((char *)tmp);
        NDRX_LOG(log_error, "Failed to alloc output UBF %ld !", Bused(tmp));
        EXFAIL_OUT(ret);
    }

    if (EXSUCCEED!=Bcpy(newbuf_out, tmp))
    {
        tpfree((char *)tmp);
        tpfree((char *)newbuf_out);

        NDRX_LOG(log_error, "Failed to copy tmp UBF to output: %s !", Bstrerror(Berror));
        EXFAIL_OUT(ret);

    }

    tmp_b=ndrx_find_buffer((char *)newbuf_out);
    tmp_b->autoalloc = (*buffer)->autoalloc;

    /* Kill the buffers */
    tpfree((*buffer)->buf);
    tpfree((char *)tmp);

    /* finally return the buffer */
    NDRX_LOG(log_info, "Returning new buffer %p", tmp_b);
    *buffer = tmp_b;
out:
    return ret;
}


/**
 * auto-buffer convert func. ubf->json
 * @param buffer
 * @return 
 */
expublic int typed_xcvt_ubf2json(buffer_obj_t **buffer)
{
    int ret = EXSUCCEED;
    buffer_obj_t *tmp_b;
    
    char * tmp = NULL;
    char * newbuf_out = NULL; /* real output buffer */

    if (NULL==(tmp = tpalloc("JSON", NULL, ATMI_MSG_MAX_SIZE)))
    {
        NDRX_LOG(log_error, "failed to convert UBF->JSON. JSON buffer alloc fail!: %s",
                tpstrerror(tperrno));
        EXFAIL_OUT(ret);
    }

    /* Do the convert */
    ndrx_TPunset_error();
    if (EXSUCCEED!=ndrx_tpubftojson((UBFH *)(*buffer)->buf, tmp, ATMI_MSG_MAX_SIZE))
    {
        tpfree((char *)tmp);
        NDRX_LOG(log_error, "Failed to convert UBF->JSON: %s", 
                tpstrerror(tperrno));
        EXFAIL_OUT(ret);
    }

    /* Shrink the buffer (by reallocating) new! 
     * we will do the shrink because, msg Q might not have settings set for
     * max buffer size...
     */
    if (NULL==(newbuf_out = tpalloc("JSON", NULL, strlen(tmp)+1)))
    {
        tpfree((char *)tmp);
        NDRX_LOG(log_error, "Failed to alloc output JSON %ld: %s", strlen(tmp)+1, 
                tpstrerror(tperrno));
        EXFAIL_OUT(ret);
    }

    strcpy(newbuf_out, tmp);

    tmp_b=ndrx_find_buffer((char *)newbuf_out);
    tmp_b->autoalloc = (*buffer)->autoalloc;

    /* Kill the buffers */
    tpfree((*buffer)->buf);
    tpfree((char *)tmp);

    /* finally return the buffer */
    NDRX_LOG(log_info, "Returning new buffer %p", tmp_b->buf);
    *buffer = tmp_b;
out:
    return ret;
}

#endif