/*
 * servidores.c
 *
 *  Created on: 30/09/2013
 *      Author: superusuario
 */

#include "GateMutexPriTP.h"
#include <ti/sysbios/gates/GateMutexPri.h>
#include <ti/uia/events/UIABenchmark.h>
#include <xdc/runtime/Log.h>
#include <stdlib.h>


#include "servidores.h"
#include "computos.h"

/* Example of execution with priority celling protocol */
///*
GateMutexPriTP_Object S1;
GateMutexPriTP_Object S2;
//*/

/* Example of execution with priority inheritance protocol */
/*
GateMutexPri_Handle S1 ;
GateMutexPri_Handle S2 ;
*/

GateMutexPri_Params prms ;

const int n_servers = 2;

/* Insert servers in the array in order by priority celling */
void Crear_Servidores (const int TP_s1, const int TP_s2) {
    /* Example of execution with priority celling protocol */
    ///*
    GateMutexPriTP_Object **serverList = malloc(n_servers*sizeof(GateMutexPriTP_Object*));
    serverList[0] = &S2;
    serverList[1] = &S1;
    GateMutexPriTP_Instance_init (&S1, TP_s1, serverList, n_servers);
    GateMutexPriTP_Instance_init (&S2, TP_s2, serverList, n_servers);
    //*/

    /* Example of execution with priority inheritance protocol */
    /*
    GateMutexPri_Params_init (&prms) ;
    S1 = GateMutexPri_create (&prms, NULL) ;
    S2 = GateMutexPri_create (&prms, NULL) ;
    */
}


// Servidor S1
void S11 (void) {
    IArg key ;

    key = GateMutexPriTP_enter (&S1) ;
    Log_write1(UIABenchmark_start, (xdc_IArg)"S1");
    CS(4);
    Log_write1(UIABenchmark_stop, (xdc_IArg)"S1");
    GateMutexPriTP_leave (&S1, key) ;
}
void S12 (void) {
    IArg key ;

    key = GateMutexPriTP_enter (&S1) ;
    Log_write1(UIABenchmark_start, (xdc_IArg)"S1");
    CS (20) ;
    Log_write1(UIABenchmark_stop, (xdc_IArg)"S1");
    GateMutexPriTP_leave (&S1, key) ;
}

// Servidor S2
void S21 (void) {
    IArg key ;

    key = GateMutexPriTP_enter (&S2) ;
    Log_write1(UIABenchmark_start, (xdc_IArg)"S2");
    CS(2);
    Log_write1(UIABenchmark_stop, (xdc_IArg)"S2");
    GateMutexPriTP_leave (&S2, key) ;
}

void S22 (void) {
    IArg key ;

    key = GateMutexPriTP_enter (&S2) ;
    Log_write1(UIABenchmark_start, (xdc_IArg)"S2");
    CS (8) ;
    Log_write1(UIABenchmark_stop, (xdc_IArg)"S2");
    GateMutexPriTP_leave (&S2, key) ;
}

