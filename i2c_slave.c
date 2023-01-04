/**
 * @file 	i2c_slave.c
 * @author 	Ed Holmes (edholmes2232(at)gmail.com)
 * @brief 	Functions for running an I2C Slave.
 * 
 * 			Get Register:										Set Register:
 * 
 * 			| Master      | Direction | Slave          |		| Master      | Direction | Slave          |
 *			|-------------|-----------|----------------|		|-------------|-----------|----------------|
 *			| Address + W | --------> | AddrCallback   |		| Address + W | --------> | AddrCallback   |
 *			| Data        | --------> | RxCpltCallback |		| Data        | --------> | RxCpltCallback |
 *			| STOP        | --------> | ListenCallback |		| Data        | --------> | RxCpltCallback |
 *			| Addr + R    | --------> | AddrCallback   |		| Data        | --------> | RxCpltCallback |
 *			| Data        | <-------- | TxCpltCallback |		  ...                                    
 *			| STOP        | --------> | ListenCallback |		| STOP        | --------> | ListenCallback |
 *
 * @todo DEVICe IS READY FIX. Maybe EnableListen_IT in stm32l0xx_it.c:157
 * @date 2022-11-14
 */

#include "i2c_slave.h"
#include "i2c.h"
#include "stdio.h"

enum SLAVE_MODE_T {
	MODE_RECEIVING,
	MODE_TRANSMITTING,
	MODE_LISTENING,
} slaveMode;

#define BUFFER_SIZE		5

static uint8_t rxBuff[BUFFER_SIZE] = {0};
static uint8_t rxBuffDataSize = 0;

static uint8_t txBuff[BUFFER_SIZE] = {0};
static uint8_t txBuffDataSize = 0;

#define GET_VOLTAGE_REG		0x08
#define SET_VOLTAGE_REG		0x09


static uint8_t requestedReg;

static uint16_t fakeVoltage;
/**
 * @brief Start listening for addr
 * 
 */
void I2C_SLAVE_Init(void) {
	printf("Enabling Listen IRQ\r\n");
	HAL_I2C_EnableListen_IT(&hi2c1);

	requestedReg = 0;
	fakeVoltage = 3542;
	slaveMode = MODE_LISTENING;
}


/**
  * @brief  Slave Address Match callback. Called when our address sent over I2C.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  TransferDirection: Master request Transfer Direction (Write/Read), value of @ref I2C_XferOptions_definition
  * @param  AddrMatchCode: Address Match Code
  * @retval None
  */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
	if (TransferDirection == I2C_DIRECTION_RECEIVE) {
		printf("Addr callback Rx\r\n");
		slaveMode = MODE_TRANSMITTING;
		switch (requestedReg) {
			case GET_VOLTAGE_REG:
				txBuff[0] = fakeVoltage >> 8;
				txBuff[1] = fakeVoltage & 0xFF;
				txBuffDataSize = 2;
				break;
			default:
				txBuff[0] = 0xFF;
				txBuff[1] = 0xFF;
				txBuffDataSize = 2;
				break;
		}

		HAL_I2C_Slave_Seq_Transmit_IT(hi2c, txBuff, txBuffDataSize, I2C_LAST_FRAME);
		requestedReg = 0;

	} else {
		printf("Addr callback Tx\r\n");
		slaveMode = MODE_RECEIVING;
		HAL_I2C_Slave_Seq_Receive_IT(hi2c, rxBuff + rxBuffDataSize, 1, I2C_NEXT_FRAME);
	}
}


/**
 * @brief Callback when Listen mode has completed (STOP condition issued)
 * 
 * @param hi2c Pointer to I2C_HandleTypeDef structure
 */
void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c) {
	printf("Listening callback\r\n");
	slaveMode = MODE_LISTENING;
	if (rxBuffDataSize == 1) {
		/* Just want register value */
		printf("Just register requested\r\n");
		requestedReg = rxBuff[0];
	} else if (rxBuffDataSize > 1) {
		/* Data sent */
		for (uint8_t i = 0; i != rxBuffDataSize; i++) {
			printf("Rx Buff[%d]: %d\r\n", i, rxBuff[i]);
		}
		requestedReg = rxBuff[0];
		fakeVoltage = (rxBuff[1] << 8 | rxBuff[2] & 0xFF);
		printf("Voltage set to %d\r\n", fakeVoltage);
	}

	memset(rxBuff, 0, BUFFER_SIZE);
	rxBuffDataSize = 0;
	
	HAL_I2C_EnableListen_IT(hi2c);
}

/**
 * @brief Callback when Receive complete (master -> slave)
 * 
 * @param hi2c Pointer to I2C_HandleTypeDef structure
 */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c) {
	printf("Rx Complete Callback\r\n");
	printf("Data in: %d\r\n", rxBuff[rxBuffDataSize]);
	rxBuffDataSize++;
	/*
	if (rxBuffDataSize >= BUFFER_SIZE) {
		printf("Rx buff full\r\n");
		return;
	}
	rxBuffDataSize++;
	*/
	
	if (slaveMode == MODE_RECEIVING) {
		HAL_I2C_Slave_Seq_Receive_IT(hi2c, rxBuff + rxBuffDataSize, 1, I2C_NEXT_FRAME);
	}
}

/**
 * @brief Callback when Transfer complete (slave -> master)
 * 
 * @param hi2c Pointer to I2C_HandleTypeDef structure
 */
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c) {
	printf("Tx Complete callback\r\n");
	txBuffDataSize = 0;
}

/**
 * @brief Callback when error condition occurs
 * 
 * @param hi2c Pointer to I2C_HandleTypeDef structure
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
	printf("Error callback\r\n");
	HAL_I2C_EnableListen_IT(hi2c);
}
