/******************************************************************************/
/*                                                                            */
/* project  : TRABAJO ASIGNATURA LAB. EMPOTRADOS                              */
/* filename : tasks_e1_Nodo_1.c                                               */
/* version  : 1                                                               */
/* date     : 31/05/2021                                                      */
/* author   : José Manuel Vidarte Llera                                       */
/* description : Planificación de tareas Nodo 1 que se comunican por bus CAN  */
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
#include <xdc/runtime/Timestamp.h>

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
Task_Handle tsk12;
Task_Handle tsk13;
Task_Handle tsk2;
Task_Handle tsk3;
Task_Handle init_syn;

Semaphore_Handle task1_sem ;
Semaphore_Handle task12_sem ;
Semaphore_Handle task13_sem ;
Semaphore_Handle task2_sem ;
Semaphore_Handle task3_sem ;
Semaphore_Handle init_sync_sem ;

Clock_Handle clocktask1 ;
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
tCANMsgObject CANMsgObject5_aux;

// Tiempos de cómputo.
uint32_t CS_task1_nodo1 = 5;
uint32_t CS_task1_nodo2 = 5;
uint32_t CS_task2_nodo3 = 25;

uint32_t periodo_T1 = 30;

// IDs de los mensajes que esperan las tareas de este nodo
uint32_t task1_ID = 0x1001;
uint32_t task1_fin12_ID = 0x1120;
uint32_t task1_fin13_ID = 0x1130;
uint32_t task2_ID = 0x2001;
uint32_t task3_ID = 0x3001;
uint32_t init_ID = 0x4000;

// Para medidas de tiempo
int clock_rate;
uint32_t t1, t2, t3, t4, usecs;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void task1(UArg arg0, UArg arg1);
Void task12(UArg arg0, UArg arg1);
Void task13(UArg arg0, UArg arg1);
Void task2(UArg arg0, UArg arg1);
Void task3(UArg arg0, UArg arg1);
Void init_sync(UArg arg0, UArg arg1);

Void task1_release (void);
Void task12_release (void);
Void task13_release (void);
Void task2_release (void);
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
        //UARTprintf("Recibido can3\n");
        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        g_bRXFlag1 = 1; // marca flag de confirmación de mensaje enviado.


    }else if(ui32Status == 2){ // Check if the cause is message object 2

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 2);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 2 en el nodo 1,
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
        //UARTprintf("Recibida mensaje CAN 3\n");

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 3 en el nodo 1,
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

        CANMessageGet(CAN0_BASE, 5, &CANMsgObject5_aux, 0);

        // Liberar la tarea para visualizar el fin global de T1 en nodo 2 y nodo 3
        if(CANMsgObject5_aux.ui32MsgID == task1_fin12_ID){
            task12_release();
        }else if(CANMsgObject5_aux.ui32MsgID == task1_fin13_ID){
            task13_release();
        }else{
            g_bErrFlag = 1;
        }
    }else if(ui32Status == 6){ // Check if the cause is message object 6

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 6);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        g_bRXFlag6 = 1; // marca flag de confirmación de mensaje enviado.
    }else if(ui32Status == 7){ // Check if the cause is message object 7

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 7);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        g_bRXFlag7 = 1; // marca flag de confirmación de mensaje enviado.
    }
    //
    // Otherwise, something unexpected caused the interrupt.  This should
    // never happen.
    //
    else{
        //UARTprintf("No encaja msj\n")
    }
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
    clock_rate = SysCtlClockGet();
    // Inicializa la consola del puerto serie para mostrar mensajes (depurar).
    InitConsole();

    // Inicializa el periférico CAN0
    InitCAN0(CANIntHandler);

    // Task T11 -- Para ver en que instante acaba la tarea T1 en nodo 2(prioridad máxima entre todas las tareas)
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk12 = Task_create (task12, &taskParams, NULL);
    task12_sem = Semaphore_create(0, NULL, NULL);

    // Task T13 -- Para ver en que instante acaba la tarea T1 en nodo 3(prioridad máxima entre todas las tareas)
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk13 = Task_create (task13, &taskParams, NULL);
    task13_sem = Semaphore_create(0, NULL, NULL);

    // Task T1
    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    tsk1 = Task_create (task1, &taskParams, NULL);
    Clock_Params_init(&clockParams);
    clockParams.period = periodo_T1 ;
    clockParams.startFlag = FALSE;
    clocktask1 = Clock_create((Clock_FuncPtr)task1_release, 5, &clockParams, NULL);
    task1_sem = Semaphore_create(0, NULL, NULL);

    // Task T2
    Task_Params_init(&taskParams);
    taskParams.priority = 2;
    tsk2 = Task_create (task2, &taskParams, NULL);
    task2_sem = Semaphore_create(0, NULL, NULL);

    // Task T3
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    tsk3 = Task_create (task3, &taskParams, NULL);
    task3_sem = Semaphore_create(0, NULL, NULL);

    //Inicializar mensajes de espera para disparar tareas
    wait_task(2, &CANMsgObject2_task2, task2_ID, 0xffff); // Dispara ejecución de tarea 2
    wait_task(3, &CANMsgObject3_task3, task3_ID, 0xffff); // Dispara ejecución de tarea 3
    wait_task(5, &CANMsgObject5_aux, 0x1000, 0xf00f); // Dispara ejecución de la tarea 12 o 13, para ver el instante global de finalización de la tarea T1

    // Enviar mensaje de sincronización a los nodos 2 y 3 para que activen el reloj que dispara su tarea a la vez.
    // Programar la tarea de inicialización
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
 *  ======== task12 ========
 */

void task12_release (void) {
      Semaphore_post(task12_sem);
}

Void task12(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task12_sem, BIOS_WAIT_FOREVER) ;

    }
}

/*
 *  ======== task13 ========
 */

void task13_release (void) {
      Semaphore_post(task13_sem);
}

Void task13(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task13_sem, BIOS_WAIT_FOREVER) ;

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

        // Ejecuto carga de trabajo artificial de la tarea 1, nodo 1
        CS (CS_task1_nodo1) ;

        //Enviar msj de continuación de tarea 1 a nodo 2 (id 0x1002)
        //UARTprintf("Nodo 1: Envio msg a nodo 2 para que continue con la tarea 1 y me quedo esperando confirmación.\n");
        //t1 = Timestamp_get32();
        send_task(1, &CANMsgObject1_task1, 0x1002, 0xffff, CS_task1_nodo2);

        while(!g_bRXFlag1){}
        //t2 = Timestamp_get32();
        //usecs = (t2 - t1) / (clock_rate/1000000);
        //UARTprintf("Envio de mensaje tarda: %u us.\n", usecs);
        g_bRXFlag1 = 0; // Limpiar flag de recepción msg1
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


        //t1 = Timestamp_get32();

        uint32_t ui32MsgData;

        CANMsgObject2_task2.pui8MsgData = (uint8_t *)&ui32MsgData;

        CANMessageGet(CAN0_BASE, 2, &CANMsgObject2_task2, 0); //Leer los datos del mensaje

        // Carga de trabajo artificial de la tarea 2, nodo 1
        CS ((int)*CANMsgObject2_task2.pui8MsgData) ;

        //Enviar msj de continuación de tarea 2 a nodo 3 (id 0x2003)
        //UARTprintf("Nodo 1: Envio msg a nodo 3 para que continue con la tarea 2.\n");
        //t2 = Timestamp_get32();
        send_task(2, &CANMsgObject2_task2, 0x2003, 0xffff, CS_task2_nodo3);

        while(!g_bRXFlag2){}
        //t3 = Timestamp_get32();
        g_bRXFlag2 = 0; // Limpiar flag de recepción msg2

        //UARTprintf("Nodo 1: Envio msg a nodo 2 para que se notifique fin de tarea 2.\n");
        send_task(6, &CANMsgObject2_task2, 0x2210, 0xffff, 0);

        while(!g_bRXFlag6){}
        g_bRXFlag6 = 0; // Limpiar flag de recepción msg2

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_2 para que espere el próximo mensaje del nodo 2.
        wait_task(2, &CANMsgObject2_task2, task2_ID, 0xffff);

        /*
        t4 = Timestamp_get32();
        usecs = (t3 - t2) / (clock_rate/1000000);
        UARTprintf("Enviar un mensaje tarda: %u usecs.\n", usecs);
        usecs = ((t4 - t1) / (clock_rate/1000000)) - 15000 - usecs;
        UARTprintf("tarea 2 total: %u usecs.\n", ((t4 - t1) / (clock_rate/1000000)));
        UARTprintf("tarea 2 cs: %u usecs.\n", 15000);
        UARTprintf("Overhead por tratamiento de mensaje: %u usecs.\n", usecs);
        */
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

        //UARTprintf("Nodo 1: Recibo mensaje de que el nodo 2 ha acabado con su parte de la tarea 3, ejecuto mi parte.\n");
        // Carga de trabajo artificial de la tarea 3, nodo 1
        CS ((int)*CANMsgObject3_task3.pui8MsgData) ;

        //Enviar msj de fin de tarea 3 a nodo 3 (id 0x3003)
        //UARTprintf("Nodo 1: Envio msg a nodo 3 para que se notifique fin de tarea 3.\n");
        send_task(7, &CANMsgObject3_task3, 0x3310, 0xffff, 0);

        while(!g_bRXFlag7){}
        g_bRXFlag7 = 0; // Limpiar flag de recepción msg3

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_3 para que espere el próximo mensaje del nodo 2.
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

    // Desactivar el reloj que dispara esta tarea inicial para que no se vuelva a ejecutar
    Clock_stop(clock_init_sync);

    //UARTprintf("Envio mensaje de sincro inicial.\n");
    send_task(4, &CANMsgObject4_Init, init_ID, 0xffff, 0);

    while(!g_bRXFlagInit){}
    g_bRXFlagInit = 0; // Limpiar flag de recepción msg4, sincronización inicial

    // Activar el reloj que dispara la tarea 1 de forma periódica en este nodo.
    Clock_start(clocktask1);

    //UARTprintf("sincro hecha.\n");
}

