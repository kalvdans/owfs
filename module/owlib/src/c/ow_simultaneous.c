/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: palfille@earthlink.net
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

/* General Device File format:
    This device file corresponds to a specific 1wire/iButton chip type
    ( or a closely related family of chips )

    The connection to the larger program is through the "device" data structure,
      which must be declared in the acompanying header file.

    The device structure holds the
      family code,
      name,
      device type (chip, interface or pseudo)
      number of properties,
      list of property structures, called "filetype".

    Each filetype structure holds the
      name,
      estimated length (in bytes),
      aggregate structure pointer,
      data format,
      read function,
      write funtion,
      generic data pointer

    The aggregate structure, is present for properties that several members
    (e.g. pages of memory or entries in a temperature log. It holds:
      number of elements
      whether the members are lettered or numbered
      whether the elements are stored together and split, or separately and joined
*/

/* Simultaneous is a trigger to do a mass conversion on all the devices in the specified path */

#include <config.h>
#include "owfs_config.h"
#include "ow_simultaneous.h"

/* ------- Prototypes ----------- */
/* Statistics reporting */
 yREAD_FUNCTION( FS_r_convert ) ;
yWRITE_FUNCTION( FS_w_convert ) ;

/* -------- Structures ---------- */
struct filetype simultaneous[] = {
    {"temperature"     ,  1, NULL  , ft_yesno, fc_volatile, {y:FS_r_convert}, {y:FS_w_convert}, {i: simul_temp} , } ,
    {"voltage"         ,  1, NULL  , ft_yesno, fc_volatile, {y:FS_r_convert}, {y:FS_w_convert}, {i: simul_volt} , } ,
} ;
DeviceEntry( simultaneous, simultaneous ) ;

/* ------- Functions ------------ */
struct internal_prop ipSimul[] = {
    {"temperature",fc_volatile},
    {"voltage",fc_volatile},
};

/* returns 0 if valid conversion exists */
int Simul_Test( const enum simul_type type, UINT msec, const struct parsedname * pn ) {
    struct parsedname pn2 ;
    struct timeval tv,now ;
    long int diff ;
    int ret ;
    memcpy( &pn2, pn , sizeof(struct parsedname)) ; // shallow copy
    FS_LoadPath(pn2.sn,&pn2) ;
    if ( (ret=Cache_Get_Internal_Strict(&tv, sizeof( struct timeval ), &ipSimul[type],&pn2)) ) {
        LEVEL_DEBUG("No simultaneous conversion valid.\n") ;
        return ret ;
    }
    LEVEL_DEBUG("Simultaneous conversion IS valid.\n") ;
    gettimeofday(&now, NULL) ;
    diff =  1000*(now.tv_sec-tv.tv_sec) + (now.tv_usec-tv.tv_usec)/1000 ;
    if ( diff<(long int)msec ) UT_delay(msec-diff) ;
    return 0 ;
}

static int FS_w_convert(const int * y , const struct parsedname * pn) {
    struct parsedname pn2 ;
    enum simul_type type = (enum simul_type) pn->ft->data.i ;
    memcpy( &pn2, pn , sizeof(struct parsedname)) ; // shallow copy
    FS_LoadPath(pn2.sn,&pn2) ;
    pn2.dev = NULL ; /* only branch select done, not actual device */
    /* Since writing to /simultaneous/temperature is done recursive to all
    * adapters, we have to fake a successful write even if it's detected
    * as a bad adapter. */
    if ( y[0] ) {
        if ( pn->in->Adapter != adapter_Bad ) {
            int ret = 1 ; // set just to block compiler errors
            switch ( type ) {
                case simul_temp: {
                    const BYTE cmd_temp[] = { 0xCC, 0x44 } ;
                    struct transaction_log t[] = {
                        TRXN_START,
                        {cmd_temp, NULL, 2, trxn_match, } ,
                        TRXN_END ,
                    } ;
                    ret = BUS_transaction(t,&pn2) ;
                    //printf("CONVERT (simultaneous temp) ret=%d\n",ret) ;
                }
                break ;
                case simul_volt: {
                    BYTE cmd_volt[] = { 0xCC, 0x3C, 0x0F, 0x00, 0xFF, 0xFF } ;
                    struct transaction_log t[] = {
                        TRXN_START,
                        {cmd_volt, NULL, 4, trxn_match, } ,
                        {NULL, &cmd_volt[4], 2, trxn_read, } ,
                        TRXN_END ,
                    } ;
                    ret = BUS_transaction(t,&pn2) || CRC16( &cmd_volt[1],5 ) ;
                    //printf("CONVERT (simultaneous volt) ret=%d\n",ret) ;
                }
                break ;
            }
            if ( ret==0 ) {
                struct timeval tv ;
                gettimeofday(&tv, NULL) ;
                Cache_Add_Internal(&tv,sizeof(struct timeval),&ipSimul[type],&pn2) ;
            }
        }
    } else {
        //printf("CONVERT (simulateous) turning off\n") ;
        Cache_Del_Internal(&ipSimul[type],&pn2) ;
    }
    return 0 ;
}

static int FS_r_convert(int * y , const struct parsedname * pn) {
    struct parsedname pn2 ;
    struct timeval tv ;
    memcpy( &pn2, pn , sizeof(struct parsedname)) ; // shallow copy
    FS_LoadPath(pn2.sn,&pn2) ;
    y[0] = ( Cache_Get_Internal_Strict(&tv,sizeof(struct timeval),&ipSimul[pn->ft->data.i],&pn2) == 0 ) ;
    return 0 ;
}
