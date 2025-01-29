/******************************************************************************/
/*                                                                            */
/* project  : TRABAJO ASIGNATURA LAB. EMPOTRADOS                              */
/* filename : tasks_Nodo_2.c.c                                                         */
/* version  : 1                                                               */
/* date     : 31/05/2021                                                      */
/* author   : Jos� Manuel Vidarte Llera                                       */
/* description : Planificaci�n de tareas Nodo 2 que se comunican por bus CAN  */
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
#include <ti/sysbios/hal/hwi.h>

#include "inc/hw_can.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"

#include "driverlib/can.c"
#include "driverlib/can.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "driverlib/rom.h"

#include "computos.h"

/******************************************************************************/
/*                        Global variables                                    */
/******************************************************************************/

Task_Handle tsk1;
Task_Handle tsk2;
Task_Handle tsk3;

Clock_Handle clocktask1 ;
Clock_Handle clocktask2 ;
Clock_Handle clocktask3 ;

Semaphore_Handle task1_sem ;
Semaphore_Handle task2_sem ;
Semaphore_Handle task3_sem ;

Clock_Params clockParams;
Task_Params taskParams;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void task1(UArg arg0, UArg arg1);
Void task2(UArg arg0, UArg arg1);
Void task3(UArg arg0, UArg arg1);

Void task1_release (void);
Void task2_release (void);
Void task3_release (void);

// Flags para indicar que los mensajes se han enviado/recibido
volatile bool g_bErrFlag = 0;
volatile bool g_bRXFlag1 = 0;
volatile bool g_bRXFlag2 = 0;
volatile bool g_bRXFlag3 = 0;
volatile bool MsgObj4_init = 0;

// CAN message objects para mantener los distintos mensajes de CAN.
tCANMsgObject CANMsgObject4_init;
tCANMsgObject CANMsgObject1_task1;
tCANMsgObject CANMsgObject2_task2;
tCANMsgObject CANMsgObject3_task3;

// Tiempos de c�mputo de cada tarea que este nodo tiene que realizar.
int tcomp_n2_task1 = 8;
int tcomp_n2_task2 = 17;
int tcomp_n2_task3 = 50;

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
    }else if(ui32Status == 2){ // Check if the cause is message object 2

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 2);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag como que se ha recibido mensaje en la entrada 2 del buffer
        g_bRXFlag2 = 1;
    }else if(ui32Status == 3){ // Check if the cause is message object 3

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 3);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag como que se ha recibido mensaje en la entrada 3 del buffer
        g_bRXFlag3 = 1;
    }else if(ui32Status == 4){ // Check if the cause is message object 4

        // Clear the message object interrupt.
        CANIntClear(CAN0_BASE, 4);

        // Since the message was sent, clear any error flags.
        g_bErrFlag = 0;

        // Marcar flag como que se ha confirmado la recepci�n del mensaje inicial de sincronizaci�n
        MsgObj4_init = 1;
    }

    //
    // Otherwise, something unexpected caused the interrupt.  This should
    // never happen.
    //
    else{}
}

//*****************************************************************************
//
// This function sets up UART0 to be used for a console to display information
// as the example is running.
//
//*****************************************************************************
void
InitConsole(void)
{
    //
    // Enable GPIO port A which is used for UART0 pins.
    // TODO: change this to whichever GPIO port you are using.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Configure the pin muxing for UART0 functions on port A0 and A1.
    // This step is not necessary if your part does not support pin muxing.
    // TODO: change this to select the port/pin you are using.
    //
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);

    //
    // Enable UART0 so that we can configure the clock.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Use the internal 16MHz oscillator as the UART clock source.
    //
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Select the alternate (UART) function for these pins.
    // TODO: change this to select the port/pin you are using.
    //
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, 16000000);
}

//*****************************************************************************
//
// This function sets up CAN0, pin E4 for RX and pin E5 for E5
//
//*****************************************************************************
void
InitCAN0(void){
    // Se han elegido los pines de los puertos E4 y E5 para RX y TX del perif�rico CAN0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    // Se configuran estos pines del GPIO para que hagan la funci�n del perif�rico CAN0
    GPIOPinConfigure(GPIO_PE4_CAN0RX);
    GPIOPinConfigure(GPIO_PE5_CAN0TX);

    // Activar la funci�n alternativa a GPIO para estos pines (en este caso CAN0 como se ha seleccionado justo arriba)
    GPIOPinTypeCAN(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    // Una vez configurado el perif�rico CAN0, se habilita
    SysCtlPeripheralEnable(SYSCTL_PERIPH_CAN0);

    // Inicializa el controlador CAN
    CANInit(CAN0_BASE);

    // Configura el bit rate para el bus CAN, en este caso a 1 MHz
    // SysCtlClockGet() se utiliza para determinar el clock ratio que se est� usando para el clock del perif�rico CAN
    CANBitRateSet(CAN0_BASE, SysCtlClockGet(), 1000000);

    // Registrar la ISR para las interrupciones de CAN0 en la tabla del vector de interrupciones.
    // Internamente tambi�n llama a IntEnable(INT_CAN0) para activar las interrupciones CAN en el procesador
    // SI SE REGISTRA DE ESTA FORMA, LA IRQ SER� GESTIONADA POR EL BUILT-IN INTERRUPT CONTROLLER (NVIC)
    CANIntRegister(CAN0_BASE, CANIntHandler);

    //REGISTRAR LA INTERRUPCION EN SYSBIOS, LA IRQ SER� GESTIONADA POR �STE
    //DE ESTA FORMA NO SE ABORTA LA EJECUCI�N AL DISPARAR TAREAS DESDE LA ISR DE CAN0
/*
    Hwi_Handle hwi0;
    Hwi_Params hwiParams;
    Error_Block eb;

    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    hwiParams.enableInt = 1; // Para que habilite la interrupci�n al inicializarla

    hwi0 = Hwi_create(_CANIntNumberGet(CAN0_BASE), (Hwi_FuncPtr)CANIntHandler, &hwiParams, &eb);

    if (hwi0 == NULL) {
     System_abort("Hwi create failed");
    }
*/
    // Activar las interrupciones del controlador CAN
    CANIntEnable(CAN0_BASE, CAN_INT_MASTER | CAN_INT_ERROR | CAN_INT_STATUS);

    // Finalmente se habilita CAN para que pueda operar
    CANEnable(CAN0_BASE);
}

//*****************************************************************************
//
// Funci�n para sincronizar el inicio de los tres nodos
//
//*****************************************************************************
void
sync_init(void)
{
    // Esperar mensaje de nodo 1 para sincronizar inicio
    CANMsgObject4_init.ui32MsgID = 0;
    CANMsgObject4_init.ui32MsgIDMask = 0;
    CANMsgObject4_init.ui32Flags = MSG_OBJ_RX_INT_ENABLE;
    CANMsgObject4_init.ui32MsgLen = 8;

    //UARTprintf("Nodo 3: quedo a la espera de mensaje de inicializaci�n.\n");

    CANMessageSet(CAN0_BASE, 4, &CANMsgObject4_init, MSG_OBJ_TYPE_RX);

    while(!MsgObj4_init){}

    //UARTprintf("Nodo 3: recibo mensaje de sincronizaci�n.\n");
}


/******************************************************************************/
/*                        Main                                                */
/******************************************************************************/

Void main()
{
    // Configuraci�n del perif�rico CAN0 y UART para debug

    // Para establecer que el reloj funcione directamente con el cristal de la Tiva. ?
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // Inicializa la consola del puerto serie para mostrar mensajes (depurar).
    InitConsole();

    // Inicializa el perif�rico CAN0
    InitCAN0();

    // Esperar mensaje de nodo 1 para sincronizar inicio
    CANMsgObject4_init.ui32MsgID = 0;
    CANMsgObject4_init.ui32MsgIDMask = 0;
    CANMsgObject4_init.ui32Flags = MSG_OBJ_RX_INT_ENABLE;
    CANMsgObject4_init.ui32MsgLen = 8;

    UARTprintf("Nodo 2: quedo a la espera de mensaje de inicializaci�n.\n");

    CANMessageSet(CAN0_BASE, 4, &CANMsgObject4_init, MSG_OBJ_TYPE_RX);

    while(!MsgObj4_init){}

    UARTprintf("Nodo 2: recibo mensaje de sincronizaci�n.\n");

    // Task T1
    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    tsk1 = Task_create (task1, &taskParams, NULL);

    Clock_Params_init(&clockParams);
    clockParams.period = 100 ;
    clockParams.startFlag = TRUE;
    clocktask1 = Clock_create((Clock_FuncPtr)task1_release, 10, &clockParams, NULL);

    task1_sem = Semaphore_create(0, NULL, NULL);

    // Task T2
    Task_Params_init(&taskParams);
    taskParams.priority = 2;
    tsk2 = Task_create (task2, &taskParams, NULL);

    Clock_Params_init(&clockParams);
    clockParams.period = 200 ;
    clockParams.startFlag = TRUE;
    clocktask2 = Clock_create((Clock_FuncPtr)task2_release, 10, &clockParams, NULL);

    task2_sem = Semaphore_create(0, NULL, NULL);

    // Task T3
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    tsk3 = Task_create (task3, &taskParams, NULL);

    Clock_Params_init(&clockParams);
    clockParams.period = 400 ;
    clockParams.startFlag = TRUE;
    clocktask3 = Clock_create((Clock_FuncPtr)task3_release, 5, &clockParams, NULL);

    task3_sem = Semaphore_create(0, NULL, NULL);

    sync_init();

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

        // Esperar a que el nodo 1 acabe de ejecutar su parte de la tarea 1 (msg con id 0x1002)
        CANMsgObject1_task1.ui32MsgID = 0x1002;
        CANMsgObject1_task1.ui32MsgIDMask = 0xffff;
        CANMsgObject1_task1.ui32Flags = (MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_USE_ID_FILTER | MSG_OBJ_EXTENDED_ID);
        CANMsgObject1_task1.ui32MsgLen = 8;
        CANMessageSet(CAN0_BASE, 1, &CANMsgObject1_task1, MSG_OBJ_TYPE_RX);

        while(!g_bRXFlag1);
        g_bRXFlag1 = 0; // Limpiar flag de recepci�n msg1

        //UARTprintf("Nodo 2: Recibo mensaje de que el nodo 1 ha acabado con la primera parte de la tarea 1.\n");

        CS (tcomp_n2_task1) ;

        //Enviar msj de continuaci�n de tarea 1 a nodo 3 (id 0x1003)
        uint32_t ui32MsgData_init = 0;

        CANMsgObject1_task1.ui32MsgID = 0x1003;
        CANMsgObject1_task1.ui32MsgIDMask = 0xffff;
        CANMsgObject1_task1.ui32Flags = MSG_OBJ_TX_INT_ENABLE;
        CANMsgObject1_task1.ui32MsgLen = sizeof(ui32MsgData_init);
        CANMsgObject1_task1.pui8MsgData = (uint8_t *)&ui32MsgData_init;

        //UARTprintf("Nodo 2: Envio msg a nodo 3 para que continue con la tarea 1.\n");
        CANMessageSet(CAN0_BASE, 1, &CANMsgObject1_task1, MSG_OBJ_TYPE_TX);

        while(!g_bRXFlag1){}
        g_bRXFlag1 = 0; // Limpiar flag de recepci�n msg1

        // Esperar a que el �ltimo nodo (3) acabe de ejecutar su parte de la tarea 1 (msg con id 0x1000)
        CANMsgObject1_task1.ui32MsgID = 0x1000;
        CANMsgObject1_task1.ui32MsgIDMask = 0xffff;
        CANMsgObject1_task1.ui32Flags = (MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_USE_ID_FILTER | MSG_OBJ_EXTENDED_ID);
        CANMsgObject1_task1.ui32MsgLen = 8;
        CANMessageSet(CAN0_BASE, 1, &CANMsgObject1_task1, MSG_OBJ_TYPE_RX);

        while(!g_bRXFlag1);
        g_bRXFlag1 = 0; // Limpiar flag de recepci�n msg1

        //UARTprintf("Nodo 2: Recibo mensaje de que el �ltimo nodo (3) ha acabado con el resto de la tarea 1.\n");
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
        // Esperar a que el nodo 1 acabe de ejecutar su parte de la tarea 2 (msg con id 0x2002)
        CANMsgObject2_task2.ui32MsgID = 0x2002;
        CANMsgObject2_task2.ui32MsgIDMask = 0xffff;
        CANMsgObject2_task2.ui32Flags = (MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_USE_ID_FILTER | MSG_OBJ_EXTENDED_ID);
        CANMsgObject2_task2.ui32MsgLen = 8;
        CANMessageSet(CAN0_BASE, 2, &CANMsgObject2_task2, MSG_OBJ_TYPE_RX);

        while(!g_bRXFlag2);
        g_bRXFlag2 = 0; // Limpiar flag de recepci�n msg2

        //UARTprintf("Nodo 2: Recibo mensaje de que el nodo 1 ha acabado con su parte de la tarea 2.\n");

        CS (tcomp_n2_task2) ;

        //Enviar msj de fin de tarea 2 a nodos 1 y 3 (id 0x2000)
        uint32_t ui32MsgData_init = 0;

        CANMsgObject2_task2.ui32MsgID = 0x2000;
        CANMsgObject2_task2.ui32MsgIDMask = 0xffff;
        CANMsgObject2_task2.ui32Flags = MSG_OBJ_TX_INT_ENABLE;
        CANMsgObject2_task2.ui32MsgLen = sizeof(ui32MsgData_init);
        CANMsgObject2_task2.pui8MsgData = (uint8_t *)&ui32MsgData_init;

        //UARTprintf("Nodo 2: Envio msg a nodos 1 y 3 para que den por finalizada la tarea 2.\n");
        CANMessageSet(CAN0_BASE, 2, &CANMsgObject2_task2, MSG_OBJ_TYPE_TX);

        while(!g_bRXFlag2){}
        g_bRXFlag2 = 0; // Limpiar flag de recepci�n msg2
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

        CS (tcomp_n2_task3) ;
        //Enviar msj de continuaci�n de tarea 3 a nodo 3 (id 0x3003)
        uint32_t ui32MsgData_init = 0;

        CANMsgObject1_task1.ui32MsgID = 0x3003;
        CANMsgObject1_task1.ui32MsgIDMask = 0xffff;
        CANMsgObject1_task1.ui32Flags = MSG_OBJ_TX_INT_ENABLE;
        CANMsgObject1_task1.ui32MsgLen = sizeof(ui32MsgData_init);
        CANMsgObject1_task1.pui8MsgData = (uint8_t *)&ui32MsgData_init;

        //UARTprintf("Nodo 2: Envio msg a nodo 3 para que continue con la tarea 3 y me quedo esperando confirmaci�n.\n");
        CANMessageSet(CAN0_BASE, 3, &CANMsgObject1_task1, MSG_OBJ_TYPE_TX);

        while(!g_bRXFlag3){}
        g_bRXFlag3 = 0; // Limpiar flag de recepci�n msg3

        // Esperar a que el �ltimo nodo (1) acabe de ejecutar su parte de la tarea 3 (msg con id 0x3000)
        CANMsgObject1_task1.ui32MsgID = 0x3000;
        CANMsgObject1_task1.ui32MsgIDMask = 0xffff;
        CANMsgObject1_task1.ui32Flags = (MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_USE_ID_FILTER | MSG_OBJ_EXTENDED_ID);
        CANMsgObject1_task1.ui32MsgLen = 8;
        CANMessageSet(CAN0_BASE, 3, &CANMsgObject1_task1, MSG_OBJ_TYPE_RX);

        while(!g_bRXFlag3);
        g_bRXFlag3 = 0; // Limpiar flag de recepci�n msg3

        //UARTprintf("Nodo 2: Recibo mensaje de que el �ltimo nodo (1) ha acabado con el resto de la tarea 3.\n");

    }
}
