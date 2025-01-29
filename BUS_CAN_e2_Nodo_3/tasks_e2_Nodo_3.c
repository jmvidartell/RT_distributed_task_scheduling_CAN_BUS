/******************************************************************************/
/*                                                                            */
/* project  : TRABAJO ASIGNATURA LAB. EMPOTRADOS                              */
/* filename : tasks_e2_Nodo_3.c                                               */
/* version  : 1                                                               */
/* date     : 03/06/2021                                                      */
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

#include "CAN0_y_UART.h"

/******************************************************************************/
/*                        Global variables                                    */
/******************************************************************************/

Task_Handle tsk1;
Task_Handle tsk3;
Task_Handle tsk5;
Task_Handle init_syn;

Clock_Handle clocktask1 ;
Clock_Handle clocktask3 ;
Clock_Handle clocktask5 ;
Clock_Handle clock_init_sync ;

Semaphore_Handle task1_sem ;
Semaphore_Handle task3_sem ;
Semaphore_Handle task5_sem ;
Semaphore_Handle init_sync_sem ;

Clock_Params clockParams;
Task_Params taskParams;

// Flags para indicar que los mensajes se han enviado/recibido
volatile bool g_bErrFlag = 0;
volatile bool g_bRXFlag1 = 0;
volatile bool g_bRXFlag3 = 0;
volatile bool g_bRXFlag5 = 0;
volatile bool g_bRXFlagInit = 0;

// CAN message objects para mantener los distintos mensajes de CAN.
tCANMsgObject CANMsgObject1_task1;
tCANMsgObject CANMsgObject3_task3;
tCANMsgObject CANMsgObject5_task5;
tCANMsgObject CANMsgObject6_Init;

// IDs de los mensajes que disparan las tareas de este nodo
uint32_t task1_ID = 0x1003;
uint32_t task3_ID = 0x3003;
uint32_t task5_ID = 0x5003;
uint32_t init_ID = 0x6000;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void task1(UArg arg0, UArg arg1);
Void task3(UArg arg0, UArg arg1);
Void task5(UArg arg0, UArg arg1);
Void init_sync(UArg arg0, UArg arg1);

Void task1_release (void);
Void task3_release (void);
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

        // Marcar flag como que se ha recibido mensaje en la entrada 1 del buffer
        g_bRXFlag1 = 1;
    }else if(ui32Status == 3){ // Check if the cause is message object 3

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 3);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag como que se ha recibido mensaje en la entrada 3 del buffer
        g_bRXFlag3 = 1;
    }else if(ui32Status == 5){ // Check if the cause is message object 5

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 5);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag como que se ha recibido mensaje en la entrada 5 del buffer
        g_bRXFlag5 = 1;
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

    // Task T1
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    tsk1 = Task_create (task1, &taskParams, NULL);

	Clock_Params_init(&clockParams);
	clockParams.period = 250 ;
	clockParams.startFlag = FALSE;
	clocktask1 = Clock_create((Clock_FuncPtr)task1_release, 85, &clockParams, NULL);

    task1_sem = Semaphore_create(0, NULL, NULL);

    // Task T3
    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    tsk3 = Task_create (task3, &taskParams, NULL);

	Clock_Params_init(&clockParams);
	clockParams.period = 250 ;
	clockParams.startFlag = FALSE;
	clocktask3 = Clock_create((Clock_FuncPtr)task3_release, 38, &clockParams, NULL);

    task3_sem = Semaphore_create(0, NULL, NULL);

    // Task T5
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    tsk5 = Task_create (task5, &taskParams, NULL);

    Clock_Params_init(&clockParams);
    clockParams.period = 250 ;
    clockParams.startFlag = FALSE;
    clocktask5 = Clock_create((Clock_FuncPtr)task5_release, 5, &clockParams, NULL);

    task5_sem = Semaphore_create(0, NULL, NULL);

    // Esperar mensaje de sincronización inicial del nodo 1
    // Programar la tarea de espera de sincronización para dentro de 5 ms
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

        //UARTprintf("Nodo 3: Envio msg a nodo 1 para que ejecute la tarea 1 y me quedo esperando confirmación.\n");
        send_task(1, &CANMsgObject1_task1, 0x1001, 0xffff);

        while(!g_bRXFlag1){}
        g_bRXFlag1 = 0; // Limpiar flag de recepción msg1

        // Esperar a que nodo 1 acabe de ejecutar la tarea 1 (msg con id 0x1003)
        wait_task(1, &CANMsgObject1_task1, task1_ID, 0xffff);

        while(!g_bRXFlag1);
        g_bRXFlag1 = 0; // Limpiar flag de recepción msg1

        //UARTprintf("Nodo 3: Recibo mensaje de el nodo 1 ha acabado de ejecutar la tarea 1.\n");
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

        //UARTprintf("Nodo 3: Envio msg a nodo 1 para que ejecute la tarea 3 y me quedo esperando confirmación.\n");
        send_task(3, &CANMsgObject3_task3, 0x3001, 0xffff);

        while(!g_bRXFlag3){}
        g_bRXFlag3 = 0; // Limpiar flag de recepción msg3

        // Esperar a que nodo 1 acabe de ejecutar la tarea 3 (msg con id 0x3003)
        wait_task(3, &CANMsgObject3_task3, task3_ID, 0xffff);

        while(!g_bRXFlag3){}
        g_bRXFlag3 = 0; // Limpiar flag de recepción msg3

        //UARTprintf("Nodo 3: Recibo mensaje de el nodo 1 ha acabado de ejecutar la tarea 3.\n");
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

        //UARTprintf("Nodo 3: Envio msg a nodo 1 para que ejecute la tarea 5 y me quedo esperando confirmación.\n");
        send_task(5, &CANMsgObject5_task5, 0x5001, 0xffff);

        while(!g_bRXFlag5){}
        g_bRXFlag5 = 0; // Limpiar flag de recepción msg5

        // Esperar a que nodo 1 acabe de ejecutar la tarea 5 (msg con id 0x5003)
        wait_task(5, &CANMsgObject5_task5, task5_ID, 0xffff);

        while(!g_bRXFlag5){}
        g_bRXFlag5 = 0; // Limpiar flag de recepción msg5

        //UARTprintf("Nodo 3: Recibo mensaje de el nodo 1 ha acabado de ejecutar la tarea 5.\n");
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
    wait_task(6, &CANMsgObject6_Init, init_ID, 0xffff);
    while(!g_bRXFlagInit){}
    g_bRXFlagInit = 0; // Limpiar flag de recepción msg6, sincronización inicial

    //Activar relojes que disparan las tareas
    Clock_start(clocktask1);
    Clock_start(clocktask3);
    Clock_start(clocktask5);

    //UARTprintf("sincro hecha.\n");
}
