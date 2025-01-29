/*
 * CAN0_y_UART.h
 *
 *  Created on: 04/06/2021
 *      Author: José Manuel Vidarte Llera
 */

#ifndef CAN0_Y_UART_H_
#define CAN0_Y_UART_H_

void InitConsole(void);

void InitCAN0(void (*CANIntHandler)(void));

void wait_task(uint32_t msg_Obj_ID, tCANMsgObject *CANMsgObject_task, uint32_t task_ID, uint32_t ID_mask);

void send_task(uint32_t msg_Obj_ID, tCANMsgObject *CANMsgObject_task, uint32_t task_ID, uint32_t ID_mask, uint32_t ui32MsgData);

#endif /* CAN0_Y_UART_H_ */
