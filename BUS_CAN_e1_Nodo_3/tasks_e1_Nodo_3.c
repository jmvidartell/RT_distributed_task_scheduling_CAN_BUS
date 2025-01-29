/******************************************************************************/
/*                                                                            */
/* project  : TRABAJO ASIGNATURA LAB. EMPOTRADOS                              */
/* filename : tasks_e1_Nodo_3.c                                               */
/* version  : 1                                                               */
/* date     : 31/05/2021                                                      */
/* author   : José Manuel Vidarte Llera                                       */
/* description : Planificación de tareas Nodo 3 que se comunican por bus CAN  */
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

Task_Handle tsk0;
Task_Handle tsk1;
Task_Handle tsk2;
Task_Handle tsk3;
Task_Handle tsk31;
Task_Handle tsk32;
Task_Handle init_syn;

Semaphore_Handle task0_sem ;
Semaphore_Handle task1_sem ;
Semaphore_Handle task2_sem ;
Semaphore_Handle task3_sem ;
Semaphore_Handle task31_sem ;
Semaphore_Handle task32_sem ;
Semaphore_Handle init_sync_sem ;

Clock_Handle clocktask3 ;
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

// CAN message objects para mantener los distintos mensajes de CAN.
tCANMsgObject CANMsgObject1_task1;
tCANMsgObject CANMsgObject2_task2;
tCANMsgObject CANMsgObject3_task3;
tCANMsgObject CANMsgObject4_Init;
tCANMsgObject CANMsgObject7_aux;

// Tiempos de cómputo de cada tarea que este nodo tiene que realizar.
uint32_t CS_task3_nodo2 = 35;
uint32_t CS_task3_nodo3 = 15;

uint32_t periodo_T3 = 120;

// IDs de los mensajes que disparan las tareas de este nodo
uint32_t task1_ID = 0x1003;
uint32_t task2_ID = 0x2003;
uint32_t task3_ID = 0x3003;
uint32_t task3_fin31_ID = 0x3310;
uint32_t task3_fin32_ID = 0x3320;
uint32_t init_ID = 0x4000;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void task1(UArg arg0, UArg arg1);
Void task2(UArg arg0, UArg arg1);
Void task3(UArg arg0, UArg arg1);
Void task31(UArg arg0, UArg arg1);
Void task32(UArg arg0, UArg arg1);
Void init_sync(UArg arg0, UArg arg1);

Void task1_release (void);
Void task2_release (void);
Void task3_release (void);
Void task31_release (void);
Void task32_release (void);
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

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 1 en el nodo 3,
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

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 2 en el nodo 3,
        // en ese caso se dispara.
        if(CANMsgObject2_task2.ui32MsgID == task2_ID){
            task2_release();
        }else{
            g_bRXFlag2 = 1; // Caso contrario, marca flag de confirmación de mensaje enviado.
        }
    }else if(ui32Status == 3){ // Check if the cause is message object 3

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 3);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        g_bRXFlag3 = 1;
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

        // Marcar flag de envio correcto de mensaje.
        g_bRXFlag5 = 1;
    }else if(ui32Status == 6){ // Check if the cause is message object 6

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 6);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag de envio correcto de mensaje.
        g_bRXFlag6 = 1;
    }else if(ui32Status == 7){ // Check if the cause is message object 7

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 7);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        CANMessageGet(CAN0_BASE, 7, &CANMsgObject7_aux, 0);

        // Liberar la tarea para visualizar el fin global de T3 en nodo 1 y nodo 2
        if(CANMsgObject7_aux.ui32MsgID == task3_fin31_ID){
            task31_release();
        }else if(CANMsgObject7_aux.ui32MsgID == task3_fin32_ID){
            task32_release();
        }else{
            g_bErrFlag = 1;
        }
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

    // Task T31 -- Para ver en que instante acaba la tarea T3 en nodo 1(prioridad máxima entre todas las tareas)
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk31 = Task_create (task31, &taskParams, NULL);
    task31_sem = Semaphore_create(0, NULL, NULL);

    // Task T32 -- Para ver en que instante acaba la tarea T3 en nodo 2(prioridad máxima entre todas las tareas)
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk32 = Task_create (task32, &taskParams, NULL);
    task32_sem = Semaphore_create(0, NULL, NULL);

    // Task T1
    Task_Params_init(&taskParams);
    taskParams.priority = 2;
    tsk1 = Task_create (task1, &taskParams, NULL);
    task1_sem = Semaphore_create(0, NULL, NULL);

    // Task T2
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    tsk2 = Task_create (task2, &taskParams, NULL);
    task2_sem = Semaphore_create(0, NULL, NULL);

    // Task T3
    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    tsk3 = Task_create (task3, &taskParams, NULL);
    Clock_Params_init(&clockParams);
    clockParams.period = periodo_T3 ;
    clockParams.startFlag = FALSE;
    clocktask3 = Clock_create((Clock_FuncPtr)task3_release, 5, &clockParams, NULL);
    task3_sem = Semaphore_create(0, NULL, NULL);

    // Inicializar mensajes de espera para disparar tareas
    wait_task(1, &CANMsgObject1_task1, task1_ID, 0xffff);
    wait_task(2, &CANMsgObject2_task2, task2_ID, 0xffff);
    wait_task(7, &CANMsgObject7_aux, 0x3000, 0xf00f); // Dispara ejecución de la tarea 31 o 32, para ver el instante global de finalización de la tarea T3

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
 *  ======== task31 ========
 */

void task31_release (void) {
      Semaphore_post(task31_sem);
}

Void task31(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task31_sem, BIOS_WAIT_FOREVER) ;

    }
}

/*
 *  ======== task32 ========
 */

void task32_release (void) {
      Semaphore_post(task32_sem);
}

Void task32(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task32_sem, BIOS_WAIT_FOREVER) ;

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

        //UARTprintf("Nodo 3: Recibo mensaje de que el nodo 2 ha acabado con su parte de la tarea 1, ejecuto mi parte.\n");
        // Carga de trabajo artificial de la tarea 1, nodo 3
        CS(*CANMsgObject1_task1.pui8MsgData);

        //UARTprintf("Nodo 3: Envio msg a nodo 1 para que se notifique fin de tarea 1.\n");
        send_task(5, &CANMsgObject1_task1, 0x1130, 0xffff, 0);

        while(!g_bRXFlag5){}
        g_bRXFlag5 = 0; // Limpiar flag de recepción msg5

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_1 para que espere el próximo mensaje del nodo 2.
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

        uint32_t ui32MsgData;

        CANMsgObject2_task2.pui8MsgData = (uint8_t *)&ui32MsgData;

        CANMessageGet(CAN0_BASE, 2, &CANMsgObject2_task2, 0); //Leer los datos del mensaje

        //UARTprintf("Nodo 3: Recibo mensaje de que el nodo 1 ha acabado con su parte de la tarea, ejecuto mi parte.\n");
        // Carga de trabajo artificial de la tarea 2, nodo 3
        CS ((int)*CANMsgObject2_task2.pui8MsgData) ;

        //UARTprintf("Nodo 3: Envio msg a nodo 2 para que se notifique fin de tarea 2.\n");
        send_task(6, &CANMsgObject2_task2, 0x2230, 0xffff, 0);

        while(!g_bRXFlag6){}
        g_bRXFlag6 = 0; // Limpiar flag de recepción msg2

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_2 para que espere el próximo mensaje del nodo 1.
        wait_task(2, &CANMsgObject2_task2, task2_ID, 0xffff);
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

        // Ejecuto carga de trabajo artificial de la tarea 3, nodo 3
        CS (CS_task3_nodo3) ;

        //Enviar msj de continuación de tarea 3 a nodo 2 (id 0x3002)
        //UARTprintf("Nodo 3: Envio msg a nodo 2 para que continue con la tarea 3 y me quedo esperando confirmación.\n");
        send_task(3, &CANMsgObject3_task3, 0x3002, 0xffff, CS_task3_nodo2);

        while(!g_bRXFlag3){}
        g_bRXFlag3 = 0; // Limpiar flag de recepción msg3
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
    Clock_start(clocktask3);

    //UARTprintf("sincro hecha.\n");
}

