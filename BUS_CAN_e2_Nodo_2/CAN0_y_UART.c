/*
 * CAN0_y_UART.c
 *
 *  Created on: 04/06/2021
 *      Author: José Manuel Vidarte Llera
 */

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

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

#include "CAN0_y_UART.h"

//*****************************************************************************
//
// This function sets up UART0 to be used for a console to display information
// as the example is running.
//
//*****************************************************************************
void InitConsole(void){
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
void InitCAN0(void (*CANIntHandler)(void)){
    // Se han elegido los pines de los puertos E4 y E5 para RX y TX del periférico CAN0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    // Se configuran estos pines del GPIO para que hagan la función del periférico CAN0
    GPIOPinConfigure(GPIO_PE4_CAN0RX);
    GPIOPinConfigure(GPIO_PE5_CAN0TX);

    // Activar la función alternativa a GPIO para estos pines (en este caso CAN0 como se ha seleccionado justo arriba)
    GPIOPinTypeCAN(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    // Una vez configurado el periférico CAN0, se habilita
    SysCtlPeripheralEnable(SYSCTL_PERIPH_CAN0);

    // Inicializa el controlador CAN
    CANInit(CAN0_BASE);

    // Configura el bit rate para el bus CAN, en este caso a 1 MHz
    // SysCtlClockGet() se utiliza para determinar el clock ratio que se está usando para el clock del periférico CAN
    CANBitRateSet(CAN0_BASE, SysCtlClockGet(), 1000000);

    // Registrar la ISR para las interrupciones de CAN0 en la tabla del vector de interrupciones.
    // Internamente también llama a IntEnable(INT_CAN0) para activar las interrupciones CAN en el procesador
    // SI SE REGISTRA DE ESTA FORMA, LA IRQ SERÁ GESTIONADA POR EL BUILT-IN INTERRUPT CONTROLLER (NVIC)
    //CANIntRegister(CAN0_BASE, CANIntHandler);

    //REGISTRAR LA INTERRUPCION EN SYSBIOS, LA IRQ SERÁ GESTIONADA POR ÉSTE
    //DE ESTA FORMA NO SE ABORTA LA EJECUCIÓN AL DISPARAR TAREAS DESDE LA ISR DE CAN0
    Hwi_Handle hwi0;
    Hwi_Params hwiParams;
    Error_Block eb;

    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    hwiParams.enableInt = 1; // Para que habilite la interrupción al inicializarla

    hwi0 = Hwi_create(_CANIntNumberGet(CAN0_BASE), (Hwi_FuncPtr)CANIntHandler, &hwiParams, &eb);

    if (hwi0 == NULL) {
     System_abort("Hwi create failed");
    }

    // Activar las interrupciones del controlador CAN
    CANIntEnable(CAN0_BASE, CAN_INT_MASTER | CAN_INT_ERROR | CAN_INT_STATUS);

    // Finalmente se habilita CAN para que pueda operar
    CANEnable(CAN0_BASE);
}

//*****************************************************************************
//
// Función para inicializar el mensaje de espera (CAN MSG OBJ) que dispara
// una tarea.
//
//*****************************************************************************
void wait_task(uint32_t msg_Obj_ID, tCANMsgObject *CANMsgObject_task, uint32_t task_ID, uint32_t ID_mask){

    CANMsgObject_task->ui32MsgID = task_ID;
    CANMsgObject_task->ui32MsgIDMask = ID_mask;
    CANMsgObject_task->ui32Flags = (MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_USE_ID_FILTER | MSG_OBJ_EXTENDED_ID);
    CANMsgObject_task->ui32MsgLen = 0; //NO SE USA EL CONTENIDO DE LOS DATOS DEL MENSAJE
    CANMessageSet(CAN0_BASE, msg_Obj_ID, CANMsgObject_task, MSG_OBJ_TYPE_RX);
}

//*****************************************************************************
//
// Función para enviar un mensaje que solicita o confirma la ejecución
// de una tarea.
//
//*****************************************************************************
void send_task(uint32_t msg_Obj_ID, tCANMsgObject *CANMsgObject_task, uint32_t task_ID, uint32_t ID_mask){

    //uint32_t ui32MsgData_init = 0;
    CANMsgObject_task->ui32MsgID = task_ID;
    CANMsgObject_task->ui32MsgIDMask = ID_mask;
    CANMsgObject_task->ui32Flags = MSG_OBJ_TX_INT_ENABLE;
    CANMsgObject_task->ui32MsgLen = 0;//sizeof(ui32MsgData_init);
   //CANMsgObject_task->pui8MsgData = (uint8_t *)&ui32MsgData_init; NO SE ENVIA CONTENIDO EN LOS DATOS DEL MENSAJE
    CANMessageSet(CAN0_BASE, msg_Obj_ID, CANMsgObject_task, MSG_OBJ_TYPE_TX);
}

