/******************************************************************************/
/*                                                                            */
/* project  : TRABAJO ASIGNATURA LAB. EMPOTRADOS                              */
/* filename : tasks_e1_Nodo_2.c                                                  */
/* version  : 1                                                               */
/* date     : 31/05/2021                                                      */
/* author   : José Manuel Vidarte Llera                                       */
/* description : Planificación de tareas Nodo 2 que se comunican por bus CAN  */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                        Defines                                             */
/******************************************************************************/

#define TARGET_IS_TM4C123_RB1

/******************************************************************************/
/*                        Used modules                                        */
/******************************************************************************/

#include <xdc/std.h>
#include <stdbool.h>
#include <stdint.h>

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Log.h>

#include <ti/uia/events/UIABenchmark.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "driverlib/can.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"

#include "utils/uartstdio.h"

#include "inc/hw_memmap.h"
#include "inc/hw_can.h"

#include "computos.h"
#include "CAN0_y_UART.h"

/******************************************************************************/
/*                        Global variables                                    */
/******************************************************************************/

Task_Handle tsk1;
Task_Handle tsk2;
Task_Handle tsk21;
Task_Handle tsk23;
Task_Handle tsk3;
Task_Handle init_syn;

Semaphore_Handle task1_sem ;
Semaphore_Handle task2_sem ;
Semaphore_Handle task21_sem ;
Semaphore_Handle task23_sem ;
Semaphore_Handle task3_sem ;
Semaphore_Handle init_sync_sem ;

Clock_Handle clocktask2 ;
Clock_Handle clock_init_sync ;

Clock_Params clockParams;
Task_Params taskParams;

// Flags para indicar que los mensajes se han enviado/recibido
volatile bool g_bErrFlag = 0;
volatile bool g_bRXFlag1 = 0;
volatile bool g_bRXFlag2 = 0;
volatile bool g_bRXFlag3 = 0;
volatile bool g_bRXFlagInit = 0;
volatile bool g_bRXFlag5 = 0;
volatile bool g_bRXFlag6 = 0;
volatile bool g_bRXFlag7 = 0;

// CAN message objects para mantener los distintos mensajes de CAN.
tCANMsgObject CANMsgObject1_task1;
tCANMsgObject CANMsgObject2_task2;
tCANMsgObject CANMsgObject3_task3;
tCANMsgObject CANMsgObject4_Init;
tCANMsgObject CANMsgObject6_aux;

// Tiempos de cómputo.
uint32_t CS_task1_nodo3 = 10;
uint32_t CS_task2_nodo1 = 15;
uint32_t CS_task2_nodo2 = 10;
uint32_t CS_task3_nodo1 = 50;

uint32_t periodo_T2 = 60;

// IDs de los mensajes que disparan las tareas de este nodo
uint32_t task1_ID = 0x1002;
uint32_t task2_fin21_ID = 0x2210;
uint32_t task2_fin23_ID = 0x2230;
uint32_t task2_ID = 0x2002;
uint32_t task3_ID = 0x3002;
uint32_t init_ID = 0x4000;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void task1(UArg arg0, UArg arg1);
Void task2(UArg arg0, UArg arg1);
Void task21(UArg arg0, UArg arg1);
Void task23(UArg arg0, UArg arg1);
Void task3(UArg arg0, UArg arg1);
Void init_sync(UArg arg0, UArg arg1);

Void task1_release (void);
Void task2_release (void);
Void task21_release (void);
Void task23_release (void);
Void task3_release (void);
Void init_sync_release (void);

//*****************************************************************************
//
// This function is the interrupt handler for the CAN peripheral.  It checks
// for the cause of the interrupt, and maintains a count of all messages that
// have been transmitted.
//
//*****************************************************************************
void
CANIntHandler(void)
{
    // Read the CAN interrupt status to find the cause of the interrupt
    uint32_t ui32Status = CANIntStatus(CAN0_BASE, CAN_INT_STS_CAUSE);

    // If the cause is a controller status interrupt, then get the status
    if(ui32Status == CAN_INT_INTID_STATUS){
        //
        // Read the controller status.  This will return a field of status
        // error bits that can indicate various errors.
        ui32Status = CANStatusGet(CAN0_BASE, CAN_STS_CONTROL);

        //UARTprintf("Recibida irq del controlador CAN status: %x\n", ui32Status);

        // Set a flag to indicate some errors may have occurred.
        g_bErrFlag = 1;
    }else if(ui32Status == 1){ // Check if the cause is message object 1

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 1);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 1 en el nodo 2,
        // en ese caso se dispara.
        if(CANMsgObject1_task1.ui32MsgID == task1_ID){
            task1_release();
        }else{
            g_bRXFlag1 = 1; // Caso contrario, marca flag de confirmación de mensaje enviado.
        }
    }else if(ui32Status == 2){ // Check if the cause is message object 2

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 2);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 2 en el nodo 2,
        // en ese caso se dispara.
        /*if(CANMsgObject2_task2.ui32MsgID == task2_ID){
            task2_release();
        }else{
            g_bRXFlag2 = 1; // Caso contrario, marca flag de confirmación de mensaje enviado.
        }*/
        g_bRXFlag2 = 1;
    }else if(ui32Status == 3){ // Check if the cause is message object 3

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 3);
        //UARTprintf("Recibido can3\n");
        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 3 en el nodo 2,
        // en ese caso se dispara.
        if(CANMsgObject3_task3.ui32MsgID == task3_ID){
            task3_release();
        }else{
            g_bRXFlag3 = 1; // Caso contrario, marca flag de confirmación de mensaje enviado.
        }
    }else if(ui32Status == 4){ // Check if the cause is message object 4

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 4);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag de inicialización.
        g_bRXFlagInit = 1;
    }else if(ui32Status == 5){ // Check if the cause is message object 5

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 5);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        g_bRXFlag5 = 1;
    }else if(ui32Status == 6){ // Check if the cause is message object 6

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 6);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        CANMessageGet(CAN0_BASE, 6, &CANMsgObject6_aux, 0);

        // Liberar la tarea para visualizar el fin global de T2 en nodo 1 y nodo 3
        if(CANMsgObject6_aux.ui32MsgID == task2_fin21_ID){
            task21_release();
        }else if(CANMsgObject6_aux.ui32MsgID == task2_fin23_ID){
            task23_release();
        }else{
            g_bErrFlag = 1;
        }
    }else if(ui32Status == 7){ // Check if the cause is message object 7

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 7);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        g_bRXFlag7 = 1;
    }

    //
    // Otherwise, something unexpected caused the interrupt.  This should
    // never happen.
    //
    else{}
}

/******************************************************************************/
/*                        Main                                                */
/******************************************************************************/

Void main()
{
    // Configuración del periférico CAN0 y UART para debug

    // Para establecer que el reloj funcione directamente desde el cristal externo de la Tiva.
    //SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
    // NO HACE FALTA, YA ESTÁ CONFIGURADO EN EL FICHERO app.cfg a 16 MHz

    // Inicializa la consola del puerto serie para mostrar mensajes (depurar).
    InitConsole();

    // Inicializa el periférico CAN0
    InitCAN0(CANIntHandler);

    // Task T21 -- Para ver en que instante acaba la tarea T2 en nodo 1(prioridad máxima entre todas las tareas)
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk21 = Task_create (task21, &taskParams, NULL);
    task21_sem = Semaphore_create(0, NULL, NULL);

    // Task T23 -- Para ver en que instante acaba la tarea T2 en nodo 3(prioridad máxima entre todas las tareas)
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk23 = Task_create (task23, &taskParams, NULL);
    task23_sem = Semaphore_create(0, NULL, NULL);

    // Task T1
    Task_Params_init(&taskParams);
    taskParams.priority = 2;
    tsk1 = Task_create (task1, &taskParams, NULL);
    task1_sem = Semaphore_create(0, NULL, NULL);

    // Task T2
    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    tsk2 = Task_create (task2, &taskParams, NULL);
    Clock_Params_init(&clockParams);
    clockParams.period = periodo_T2 ;
    clockParams.startFlag = FALSE;
    clocktask2 = Clock_create((Clock_FuncPtr)task2_release, 5, &clockParams, NULL);
    task2_sem = Semaphore_create(0, NULL, NULL);

    // Task T3
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    tsk3 = Task_create (task3, &taskParams, NULL);
    task3_sem = Semaphore_create(0, NULL, NULL);

    // Inicializar mensajes de espera para disparar tareas
    wait_task(1, &CANMsgObject1_task1, task1_ID, 0xffff);
    wait_task(3, &CANMsgObject3_task3, task3_ID, 0xffff);
    wait_task(6, &CANMsgObject6_aux, 0x2000, 0xf00f); // Dispara ejecución de la tarea 21 o 23, para ver el instante global de finalización de la tarea T2

    // Esperar mensaje de sincronización inicial del nodo 1
    // Programar la tarea de espera de sincronización
    Task_Params_init(&taskParams);
    taskParams.priority = 10;
    init_syn = Task_create (init_sync, &taskParams, NULL);

    init_sync_sem = Semaphore_create(0, NULL, NULL);

    Clock_Params_init(&clockParams);
    clockParams.period = 1000;
    clockParams.startFlag = TRUE;
    clock_init_sync = Clock_create((Clock_FuncPtr)init_sync_release, 5, &clockParams, NULL);

    BIOS_start();
}

/******************************************************************************/
/*                        Tasks                                               */
/******************************************************************************/
/*
 *  ======== task21 ========
 */

void task21_release (void) {
      Semaphore_post(task21_sem);
}

Void task21(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task21_sem, BIOS_WAIT_FOREVER) ;

    }
}

/*
 *  ======== task23 ========
 */

void task23_release (void) {
      Semaphore_post(task23_sem);
}

Void task23(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task23_sem, BIOS_WAIT_FOREVER) ;

    }
}

/*
 *  ======== task1 ========
 */

void task1_release (void) {
      Semaphore_post(task1_sem);
}

Void task1(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task1_sem, BIOS_WAIT_FOREVER) ;

        uint32_t ui32MsgData;

        CANMsgObject1_task1.pui8MsgData = (uint8_t *)&ui32MsgData;

        CANMessageGet(CAN0_BASE, 1, &CANMsgObject1_task1, 0); //Leer los datos del mensaje

        //UARTprintf("Nodo 2: Recibo mensaje de que el nodo 1 ha acabado con su parte de la tarea 1, ejecuto mi parte.\n");

        // Carga de trabajo artificial de la tarea 1 nodo 2
        CS ((int)*CANMsgObject1_task1.pui8MsgData) ;

        //Enviar msj de continuación de tarea 1 a nodo 3 (id 0x1003)
        //UARTprintf("Nodo 2: Envio msg a nodo 3 para que continue con la tarea 1.\n");
        send_task(1, &CANMsgObject1_task1, 0x1003, 0xffff, CS_task1_nodo3);

        while(!g_bRXFlag1){}
        g_bRXFlag1 = 0; // Limpiar flag de recepción msg1

        //Enviar msj de fin de tarea 1 a nodo 1 (id 0x1120)
        //UARTprintf("Nodo 2: Envio msg a nodo 1 para que se notifique fin de tarea 1.\n");
        send_task(5, &CANMsgObject1_task1, 0x1120, 0xffff, 0);

        while(!g_bRXFlag5){}
        g_bRXFlag5 = 0; // Limpiar flag de recepción msg5

        //UARTprintf("Nodo 1: Finaliza tarea 1 en nodo 2.\n");

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_1 para que espere el próximo mensaje del nodo 1.
        wait_task(1, &CANMsgObject1_task1, task1_ID, 0xffff);
    }
}

/*
 *  ======== task2 ========
 */

void task2_release (void) {
      Semaphore_post(task2_sem);
}

Void task2(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task2_sem, BIOS_WAIT_FOREVER) ;

        // Ejecuto carga de trabajo artificial de la tarea 2, nodo 2
        CS (CS_task2_nodo2) ;

        //Enviar msj de continuación de tarea 2 a nodo 1 (id 0x2001)
        //UARTprintf("Nodo 2: Envio msg a nodo 1 para que continue con la tarea 2 y me quedo esperando confirmación.\n");
        send_task(2, &CANMsgObject2_task2, 0x2001, 0xffff, CS_task2_nodo1);

        while(!g_bRXFlag2){}
        g_bRXFlag2 = 0; // Limpiar flag de recepción msg2
    }
}


/*
 *  ======== task3 ========
 */

void task3_release (void) {
      Semaphore_post(task3_sem);
}

Void task3(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task3_sem, BIOS_WAIT_FOREVER) ;

        uint32_t ui32MsgData;

        CANMsgObject3_task3.pui8MsgData = (uint8_t *)&ui32MsgData;

        CANMessageGet(CAN0_BASE, 3, &CANMsgObject3_task3, 0); //Leer los datos del mensaje

        //UARTprintf("Nodo 2: Recibo mensaje de que el nodo 3 ha acabado con su parte de la tarea 3, ejecuto mi parte.\n");

        // Carga de trabajo artificial de la tarea 3, nodo 2
        CS ((int)*CANMsgObject3_task3.pui8MsgData) ;

        //Enviar msj de continuación de tarea 3 a nodo 1 (id 0x3001)
        //UARTprintf("Nodo 2: Envio msg a nodo 1 para que continue con la tarea 3.\n");
        send_task(3, &CANMsgObject3_task3, 0x3001, 0xffff, CS_task3_nodo1);

        while(!g_bRXFlag3){}
        g_bRXFlag3 = 0; // Limpiar flag de recepción msg3

        //Enviar msj de fin de tarea 3 a nodo 3 (id 0x3320)
        //UARTprintf("Nodo 2: Envio msg a nodo 3 para que se notifique fin de tarea 3.\n");
        send_task(7, &CANMsgObject2_task2, 0x3320, 0xffff, 0);

        while(!g_bRXFlag7){}
        g_bRXFlag7 = 0; // Limpiar flag de recepción msg2

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_3 para que espere el próximo mensaje del nodo 3.
        wait_task(3, &CANMsgObject3_task3, task3_ID, 0xffff);
    }
}

/*
 *  ======== Initial synchronization ========
 */

void init_sync_release (void) {
      Semaphore_post(init_sync_sem);
}

Void init_sync(UArg arg0, UArg arg1)
{
    Semaphore_pend (init_sync_sem, BIOS_WAIT_FOREVER) ;

    //Desactivar reloj que dispara la tarea de sincronizacion
    Clock_stop(clock_init_sync);

    //UARTprintf("Espero sincro.\n");
    wait_task(4, &CANMsgObject4_Init, init_ID, 0xffff);

    while(!g_bRXFlagInit){}
    g_bRXFlagInit = 0; // Limpiar flag de recepción msg4, sincronización inicial

    // Activar el reloj que dispara la tarea 1 de forma periódica en este nodo.
    Clock_start(clocktask2);

    //UARTprintf("sincro hecha.\n");
}
