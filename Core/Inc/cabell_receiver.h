#ifndef __CABELL_RECEIVER_H
#define __CABELL_RECEIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

#define CABELL_RECEIVER_CHANNEL_COUNT          16U
#define CABELL_RECEIVER_CHANNEL_MIN            1000U
#define CABELL_RECEIVER_CHANNEL_MID            1500U
#define CABELL_RECEIVER_CHANNEL_MAX            2000U

/* 连续300ms收不到有效数据就进入失联保护 */
#define CABELL_RECEIVER_FAILSAFE_TIME_MS        300U

typedef enum
{
  CABELL_RECEIVER_NOT_INITIALIZED = 0,
  CABELL_RECEIVER_BINDING,
  CABELL_RECEIVER_SEARCHING,
  CABELL_RECEIVER_CONNECTED,
  CABELL_RECEIVER_FAILSAFE,
  CABELL_RECEIVER_RADIO_ERROR,
  CABELL_RECEIVER_FLASH_ERROR
} CabellReceiver_State_t;

typedef struct
{
  /* Channel[0]到Channel[15]分别对应遥控器CH1到CH16 */
  uint16_t Channel[CABELL_RECEIVER_CHANNEL_COUNT];
  uint32_t PacketCount;
  uint32_t LastPacketTick;
  uint8_t ChannelCount;
  uint8_t ModelNumber;
  uint8_t IsConnected;
  CabellReceiver_State_t State;
} CabellReceiver_Data_t;

HAL_StatusTypeDef CabellReceiver_Init(void);
void CabellReceiver_Process(void);
void CabellReceiver_GetData(CabellReceiver_Data_t *Data);
HAL_StatusTypeDef CabellReceiver_Unbind(void);

#ifdef __cplusplus
}
#endif

#endif
