/******************************************************************************/
/*                                                                            */
/* project  : TRABAJO ASIGNATURA LAB. EMPOTRADOS                              */
/* filename : tasks_e2_Nodo_1.c                                               */
/* version  : 1                                                               */
/* date     : 03/06/2021                                                      */
/* author   : José Manuel Vidarte Llera                                       */
/* description : Planificación de tareas Nodo 1 que se comunican por bus CAN, */
/*               protocolo con techo de prioridad.                            */
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
#include "servidores.h"

/******************************************************************************/
/*                        Global variables                                    */
/******************************************************************************/

Task_Handle tsk1;
Task_Handle tsk2;
Task_Handle tsk3;
Task_Handle tsk4;
Task_Handle tsk5;
Task_Handle init_syn;

Clock_Handle clock_init_sync ;

Semaphore_Handle task1_sem ;
Semaphore_Handle task2_sem ;
Semaphore_Handle task3_sem ;
Semaphore_Handle task4_sem ;
Semaphore_Handle task5_sem ;
Semaphore_Handle init_sync_sem ;

Clock_Params clockParams;
Task_Params taskParams;

// Flags para indicar que los mensajes se han enviado/recibido
volatile bool g_bErrFlag = 0;
volatile bool g_bRXFlag1 = 0;
volatile bool g_bRXFlag2 = 0;
volatile bool g_bRXFlag3 = 0;
volatile bool g_bRXFlag4 = 0;
volatile bool g_bRXFlag5 = 0;
volatile bool g_bRXFlagInit = 0;

// CAN message objects para mantener los distintos mensajes de CAN.
tCANMsgObject CANMsgObject1_task1;
tCANMsgObject CANMsgObject2_task2;
tCANMsgObject CANMsgObject3_task3;
tCANMsgObject CANMsgObject4_task4;
tCANMsgObject CANMsgObject5_task5;
tCANMsgObject CANMsgObject6_Init;

// IDs de los mensajes que disparan las tareas de este nodo
uint32_t task1_ID = 0x1001;
uint32_t task2_ID = 0x2001;
uint32_t task3_ID = 0x3001;
uint32_t task4_ID = 0x4001;
uint32_t task5_ID = 0x5001;
uint32_t init_ID = 0x6000;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void task1(UArg arg0, UArg arg1);
Void task2(UArg arg0, UArg arg1);
Void task3(UArg arg0, UArg arg1);
Void task4(UArg arg0, UArg arg1);
Void task5(UArg arg0, UArg arg1);
Void init_sync(UArg arg0, UArg arg1);

Void task1_release (void);
Void task2_release (void);
Void task3_release (void);
Void task4_release (void);
Void task5_release (void);
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

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 1,
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

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 2,
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

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 3,
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

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 4,
        // en ese caso se dispara.
        if(CANMsgObject4_task4.ui32MsgID == task4_ID){
            task4_release();
        }else{
            g_bRXFlag4 = 1; // Caso contrario, marca flag de confirmación de mensaje enviado.
        }
    }else if(ui32Status == 5){ // Check if the cause is message object 5

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 5);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Comprobar si el id del mensaje recibido coincide con el que dispara la tarea 5,
        // en ese caso se dispara.
        if(CANMsgObject5_task5.ui32MsgID == task5_ID){
            task5_release();
        }else{
            g_bRXFlag5 = 1; // Caso contrario, marca flag de confirmación de mensaje enviado.
        }
    }else if(ui32Status == 6){ // Check if the cause is message object 6

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 6);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag de inicialización.
        g_bRXFlagInit = 1;
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

    // Inicializar mensajes de espera para disparar tareas
    wait_task(1, &CANMsgObject1_task1, task1_ID, 0xffff);
    wait_task(2, &CANMsgObject2_task2, task2_ID, 0xffff);
    wait_task(3, &CANMsgObject3_task3, task3_ID, 0xffff);
    wait_task(4, &CANMsgObject4_task4, task4_ID, 0xffff);
    wait_task(5, &CANMsgObject5_task5, task5_ID, 0xffff);

    // Task T1
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk1 = Task_create (task1, &taskParams, NULL);

    task1_sem = Semaphore_create(0, NULL, NULL);

    // Task T2
    Task_Params_init(&taskParams);
    taskParams.priority = 4;
    tsk2 = Task_create (task2, &taskParams, NULL);

    task2_sem = Semaphore_create(0, NULL, NULL);

    // Task T3
    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    tsk3 = Task_create (task3, &taskParams, NULL);

    task3_sem = Semaphore_create(0, NULL, NULL);

    // Task T4
    Task_Params_init(&taskParams);
    taskParams.priority = 2;
    tsk4 = Task_create (task4, &taskParams, NULL);

    task4_sem = Semaphore_create(0, NULL, NULL);

    // Task T5
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    tsk5 = Task_create (task5, &taskParams, NULL);

    task5_sem = Semaphore_create(0, NULL, NULL);


    /* Create the servers with their respective priority celling defined by the max prio value of tasks using their services. */
    Crear_Servidores (4, 5) ;

    // Enviar mensaje de sincronización a los nodos 2 y 3 para que comiencen a disparar las tareas empezando a la vez
    // Programar la tarea de inicialización para dentro de 5 ms
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
 *  ======== task1 ========
 */

void task1_release (void) {
	  Semaphore_post(task1_sem);
}

Void task1(UArg arg0, UArg arg1)
{
    for (;;) {

    	Semaphore_pend (task1_sem, BIOS_WAIT_FOREVER) ;

    	//UARTprintf("Nodo 1: Recibo mensaje del nodo 3 para que ejecute tarea 1.\n");

        CS (9) ;
        S21 () ;
        CS (4) ;

        //Enviar msj de fin de tarea 1 a nodo 3 (id 0x1003)
        //UARTprintf("Nodo 1: Envio msg a nodo 3 para que finalice con la tarea 1, acabo.\n");
        send_task(1, &CANMsgObject1_task1, 0x1003, 0xffff);

        while(!g_bRXFlag1){}
        g_bRXFlag1 = 0; // Limpiar flag de recepción msg1

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_1 para que espere el próximo mensaje del nodo 3.
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

    	//UARTprintf("Nodo 1: Recibo mensaje del nodo 2 para que ejecute tarea 2.\n");

    	CS (20) ;
        S11 () ;
        CS (16) ;

        //Enviar msj de fin de tarea 2 a nodo 2 (id 0x2002)
        //UARTprintf("Nodo 1: Envio msg a nodo 2 para que finalice con la tarea 2, acabo.\n");
        send_task(2, &CANMsgObject2_task2, 0x2002, 0xffff);

        while(!g_bRXFlag2){}
        g_bRXFlag2 = 0; // Limpiar flag de recepción msg2

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_2 para que espere el próximo mensaje del nodo 2.
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

    	//UARTprintf("Nodo 1: Recibo mensaje del nodo 3 para que ejecute tarea 3.\n");

    	CS (50) ;

        //Enviar msj de fin de tarea 3 a nodo 3 (id 0x3003)
        //UARTprintf("Nodo 1: Envio msg a nodo 3 para que finalice con la tarea 3, acabo.\n");
        send_task(3, &CANMsgObject3_task3, 0x3003, 0xffff);

        while(!g_bRXFlag3){}
        g_bRXFlag3 = 0; // Limpiar flag de recepción msg3

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_3 para que espere el próximo mensaje del nodo 3.
        wait_task(3, &CANMsgObject3_task3, task3_ID, 0xffff);
    }
}

/*
 *  ======== task4 ========
 */

void task4_release (void) {
      Semaphore_post(task4_sem);
}

Void task4(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task4_sem, BIOS_WAIT_FOREVER) ;

        //UARTprintf("Nodo 1: Recibo mensaje del nodo 2 para que ejecute tarea 4.\n");

        CS (22) ;
        S22 () ;
        CS (20) ;

        //Enviar msj de fin de tarea 4 a nodo 2 (id 0x4002)
        //UARTprintf("Nodo 1: Envio msg a nodo 2 para que finalice con la tarea 4, acabo.\n");
        send_task(4, &CANMsgObject4_task4, 0x4002, 0xffff);

        while(!g_bRXFlag4){}
        g_bRXFlag4 = 0; // Limpiar flag de recepción msg4

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_4 para que espere el próximo mensaje del nodo 2.
        wait_task(4, &CANMsgObject4_task4, task4_ID, 0xffff);
    }
}

/*
 *  ======== task5 ========
 */

void task5_release (void) {
      Semaphore_post(task5_sem);
}

Void task5(UArg arg0, UArg arg1)
{
    for (;;) {

        Semaphore_pend (task5_sem, BIOS_WAIT_FOREVER) ;

        //UARTprintf("Nodo 1: Recibo mensaje del nodo 3 para que ejecute tarea 5.\n");

        CS (5) ;
        S12 () ;
        CS (5) ;

        //Enviar msj de fin de tarea 5 a nodo 3 (id 0x5003)
        //UARTprintf("Nodo 1: Envio msg a nodo 3 para que finalice con la tarea 5, acabo.\n");
        send_task(5, &CANMsgObject5_task5, 0x5003, 0xffff);

        while(!g_bRXFlag5){}
        g_bRXFlag5 = 0; // Limpiar flag de recepción msg5

        //Vuelvo a configurar la entrada del buffer CAN MSG_OBJ_5 para que espere el próximo mensaje del nodo 3.
        wait_task(5, &CANMsgObject5_task5, task5_ID, 0xffff);
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
    send_task(6, &CANMsgObject6_Init, init_ID, 0xffff);

    while(!g_bRXFlagInit){}
    g_bRXFlagInit = 0; // Limpiar flag de recepción msg6, sincronización inicial
    //UARTprintf("sincro hecha.\n");

}


