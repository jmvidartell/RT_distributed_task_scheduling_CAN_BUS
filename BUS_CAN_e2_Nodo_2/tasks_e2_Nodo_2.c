/******************************************************************************/
/*                                                                            */
/* project  : TRABAJO ASIGNATURA LAB. EMPOTRADOS                              */
/* filename : tasks_e2_Nodo_2.c                                               */
/* version  : 1                                                               */
/* date     : 03/06/2021                                                      */
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

#include "CAN0_y_UART.h"

/******************************************************************************/
/*                        Global variables                                    */
/******************************************************************************/

Task_Handle tsk2;
Task_Handle tsk4;
Task_Handle init_syn;

Clock_Handle clocktask2 ;
Clock_Handle clocktask4 ;
Clock_Handle clock_init_sync ;

Semaphore_Handle task2_sem ;
Semaphore_Handle task4_sem ;
Semaphore_Handle init_sync_sem ;

Clock_Params clockParams;
Task_Params taskParams;

// Flags para indicar que los mensajes se han enviado/recibido
volatile bool g_bErrFlag = 0;
volatile bool g_bRXFlag2 = 0;
volatile bool g_bRXFlag4 = 0;
volatile bool g_bRXFlagInit = 0;

// CAN message objects para mantener los distintos mensajes de CAN.
tCANMsgObject CANMsgObject2_task2;
tCANMsgObject CANMsgObject4_task4;
tCANMsgObject CANMsgObject6_Init;

// IDs de los mensajes que disparan las tareas de este nodo
uint32_t task2_ID = 0x2002;
uint32_t task4_ID = 0x4002;
uint32_t init_ID = 0x6000;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void task2(UArg arg0, UArg arg1);
Void task4(UArg arg0, UArg arg1);
Void init_sync(UArg arg0, UArg arg1);

Void task2_release (void);
Void task4_release (void);
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
    }else if(ui32Status == 2){ // Check if the cause is message object 2

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 2);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag como que se ha recibido mensaje en la entrada 2 del buffer
        g_bRXFlag2 = 1;
    }else if(ui32Status == 4){ // Check if the cause is message object 4

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 4);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag como que se ha recibido mensaje en la entrada 2 del buffer
        g_bRXFlag4 = 1;
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

    // Task T2
    Task_Params_init(&taskParams);
    taskParams.priority = 4;
    tsk2 = Task_create (task2, &taskParams, NULL);

	Clock_Params_init(&clockParams);
	clockParams.period = 250 ;
	clockParams.startFlag = FALSE;
	clocktask2 = Clock_create((Clock_FuncPtr)task2_release, 58, &clockParams, NULL);

    task2_sem = Semaphore_create(0, NULL, NULL);

    // Task T4
    Task_Params_init(&taskParams);
    taskParams.priority = 2;
    tsk4 = Task_create (task4, &taskParams, NULL);

    Clock_Params_init(&clockParams);
    clockParams.period = 250 ;
    clockParams.startFlag = FALSE;
    clocktask4 = Clock_create((Clock_FuncPtr)task4_release, 13, &clockParams, NULL);

    task4_sem = Semaphore_create(0, NULL, NULL);

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
 *  ======== task2 ========
 */

void task2_release (void) {
	  Semaphore_post(task2_sem);
}

Void task2(UArg arg0, UArg arg1)
{
    for (;;) {

    	Semaphore_pend (task2_sem, BIOS_WAIT_FOREVER) ;

    	//UARTprintf("Nodo 2: Envio msg a nodo 1 para que ejecute la tarea 2 y me quedo esperando confirmación.\n");
        send_task(2, &CANMsgObject2_task2, 0x2001, 0xffff);

        while(!g_bRXFlag2){}
        g_bRXFlag2 = 0; // Limpiar flag de recepción msg2

        // Esperar a que nodo 1 acabe de ejecutar la tarea 2 (msg con id 0x2002)
        wait_task(2, &CANMsgObject2_task2, task2_ID, 0xffff);

        while(!g_bRXFlag2);
        g_bRXFlag2 = 0; // Limpiar flag de recepción msg2

        //UARTprintf("Nodo 2: Recibo mensaje de el nodo 1 ha acabado de ejecutar la tarea 2.\n");
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

        //UARTprintf("Nodo 2: Envio msg a nodo 1 para que ejecute la tarea 4 y me quedo esperando confirmación.\n");
        send_task(4, &CANMsgObject4_task4, 0x4001, 0xffff);

        while(!g_bRXFlag4){}
        g_bRXFlag4 = 0; // Limpiar flag de recepción msg4

        // Esperar a que nodo 1 acabe de ejecutar la tarea 4 (msg con id 0x4002)
        wait_task(4, &CANMsgObject4_task4, task4_ID, 0xffff);

        while(!g_bRXFlag4);
        g_bRXFlag4 = 0; // Limpiar flag de recepción msg4

        //UARTprintf("Nodo 2: Recibo mensaje de el nodo 1 ha acabado de ejecutar la tarea 4.\n");

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
    Clock_start(clocktask2);
    Clock_start(clocktask4);

    //UARTprintf("sincro hecha.\n");
}

