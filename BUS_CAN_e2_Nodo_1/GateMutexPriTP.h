/*
 * GateMutexPriTP.h
 *
 *  Created on: 9 ene. 2021
 *      Author: jmvid
 */

#ifndef GateMutexPriTP
#define GateMutexPriTP

#include <xdc/std.h>
#include <xdc/runtime/xdc.h>
#include <xdc/runtime/Types.h>
#include <xdc/runtime/IInstance.h>
#include <ti/sysbios/gates/package/package.defs.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Task.h>
#include <xdc/runtime/IGateProvider.h>

typedef struct GateMutexPriTP_Object GateMutexPriTP_Object;

/* GateMutexPriTP Object */
struct GateMutexPriTP_Object {
    volatile xdc_UInt mutexCnt;
    volatile xdc_Int TP;    /* Priority celling value. */
    volatile xdc_Int ownerOrigPri;
    volatile ti_sysbios_knl_Task_Handle owner;
    ti_sysbios_knl_Queue_Handle queue;
    GateMutexPriTP_Object** serverList;
    volatile xdc_Int n_servers;

};

/* GateMutexPriTP functions */
Void GateMutexPriTP_Instance_init(GateMutexPriTP_Object *obj, const xdc_Int techo_prio, GateMutexPriTP_Object** serverList, const xdc_Int n_servers);
Void GateMutexPri_Instance_finalize(GateMutexPriTP_Object *obj);
IArg GateMutexPriTP_enter(GateMutexPriTP_Object *obj);
Void GateMutexPriTP_leave(GateMutexPriTP_Object *obj, IArg key);

#endif



