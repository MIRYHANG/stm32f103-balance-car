/*
 * Cabell V3接收层，协议来源：
 * https://github.com/soligen2010/RC_RX_CABELL_V3_FHSS
 * https://github.com/pascallanger/DIY-Multiprotocol-TX-Module
 *
 * 这里只保留平衡车需要的NRF24L01接收、对频、跳频、通道解码和失联保护。
 * 舵机PWM、PPM、SBUS和遥测均未加入。
 */

#include "cabell_receiver.h"
#include "main.h"
#include "spi.h"
#include <stddef.h>
#include <string.h>

/* NRF24L01命令 */
#define NRF_CMD_R_REGISTER                 0x00U
#define NRF_CMD_W_REGISTER                 0x20U
#define NRF_CMD_R_RX_PAYLOAD               0x61U
#define NRF_CMD_FLUSH_RX                   0xE2U
#define NRF_CMD_R_RX_PL_WID                0x60U
#define NRF_CMD_ACTIVATE                   0x50U
#define NRF_CMD_NOP                        0xFFU

/* NRF24L01寄存器 */
#define NRF_REG_CONFIG                     0x00U
#define NRF_REG_EN_AA                      0x01U
#define NRF_REG_EN_RXADDR                  0x02U
#define NRF_REG_SETUP_AW                   0x03U
#define NRF_REG_SETUP_RETR                 0x04U
#define NRF_REG_RF_CH                      0x05U
#define NRF_REG_RF_SETUP                   0x06U
#define NRF_REG_STATUS                     0x07U
#define NRF_REG_RX_ADDR_P1                 0x0BU
#define NRF_REG_RX_PW_P1                   0x12U
#define NRF_REG_FIFO_STATUS                0x17U
#define NRF_REG_DYNPD                      0x1CU
#define NRF_REG_FEATURE                    0x1DU

#define NRF_STATUS_RX_DR                   0x40U
#define NRF_FIFO_RX_EMPTY                  0x01U

#define CABELL_MODE_NORMAL                 0U
#define CABELL_MODE_BIND                   1U
#define CABELL_MODE_SET_FAILSAFE           2U
#define CABELL_MODE_NORMAL_TELEMETRY       3U
#define CABELL_MODE_BIND_NO_PULSE          5U
#define CABELL_MODE_UNBIND                 127U

#define CABELL_PACKET_MAX_SIZE             30U
#define CABELL_PACKET_HEADER_SIZE          6U
#define CABELL_PAYLOAD_MAX_SIZE            24U
#define CABELL_MIN_CHANNEL_COUNT           4U
#define CABELL_RADIO_SEQUENCE_SIZE         9U
#define CABELL_RADIO_MIN_CHANNEL           3U
#define CABELL_RADIO_MAX_CHANNEL           47U

#define CABELL_PACKET_PERIOD_US            3000U
#define CABELL_PACKET_TIMEOUT_ADD_US       200U
#define CABELL_RESYNC_TIMEOUT_US            2000000U
#define CABELL_RESYNC_CHANNEL_TIME_US       212000U
#define CABELL_MAX_FAST_SWITCH_COUNT       8U

#define CABELL_FLASH_MAGIC                 0x43414233UL
#define CABELL_FLASH_VERSION               1U

/* 最后一页Flash专门保存Cabell对频数据，链接文件已经把这一页预留出来 */
extern uint8_t _cabell_storage_start[];
#define CABELL_FLASH_ADDRESS               ((uint32_t)_cabell_storage_start)

typedef struct
{
  uint32_t Magic;
  uint8_t Version;
  uint8_t Address[5];
  uint8_t ModelNumber;
  uint8_t Reserved;
  uint16_t Checksum;
} CabellFlashData_t;

static CabellReceiver_Data_t CabellData;

/* Address按发射端顺序保存，写入NRF寄存器时需要倒序 */
static const uint8_t CabellBindAddress[5] = {0xA4U, 0xB7U, 0xC1U, 0x23U, 0xF7U};
static uint8_t CabellAddress[5];
static uint8_t CabellRadioSequence[CABELL_RADIO_SEQUENCE_SIZE];
static uint8_t CabellCurrentRadioChannel;
static uint8_t CabellIsBound;
static uint8_t CabellRadioReady;
static uint8_t CabellIsResyncing;
static uint8_t CabellConsecutiveMissCount;
static uint32_t CabellPacketPeriodUs;
static uint32_t CabellNextRadioSwitchUs;
static uint32_t CabellLastValidPacketUs;

static uint32_t CabellLastCycleCount;
static uint32_t CabellMicrosecondCount;
static uint32_t CabellCycleRemainder;
static uint32_t CabellCyclesPerMicrosecond;

static void Cabell_CSN(GPIO_PinState State)
{
  HAL_GPIO_WritePin(NRF24_CSN_GPIO_Port, NRF24_CSN_Pin, State);
}

static void Cabell_CE(GPIO_PinState State)
{
  HAL_GPIO_WritePin(NRF24_CE_GPIO_Port, NRF24_CE_Pin, State);
}

static HAL_StatusTypeDef Cabell_NrfTransmitReceive(uint8_t *TxData,
                                                   uint8_t *RxData,
                                                   uint16_t Size)
{
  HAL_StatusTypeDef Status;

  Cabell_CSN(GPIO_PIN_RESET);
  Status = HAL_SPI_TransmitReceive(&hspi1, TxData, RxData, Size, 2U);
  Cabell_CSN(GPIO_PIN_SET);

  return Status;
}

static HAL_StatusTypeDef Cabell_NrfCommand(uint8_t Command)
{
  uint8_t RxData;
  return Cabell_NrfTransmitReceive(&Command, &RxData, 1U);
}

static HAL_StatusTypeDef Cabell_NrfReadRegister(uint8_t Register, uint8_t *Value)
{
  uint8_t TxData[2] = {(uint8_t)(NRF_CMD_R_REGISTER | (Register & 0x1FU)), NRF_CMD_NOP};
  uint8_t RxData[2];
  HAL_StatusTypeDef Status;

  Status = Cabell_NrfTransmitReceive(TxData, RxData, sizeof(TxData));
  if (Status == HAL_OK)
  {
    *Value = RxData[1];
  }
  return Status;
}

static HAL_StatusTypeDef Cabell_NrfWriteRegister(uint8_t Register, uint8_t Value)
{
  uint8_t TxData[2] = {(uint8_t)(NRF_CMD_W_REGISTER | (Register & 0x1FU)), Value};
  uint8_t RxData[2];
  return Cabell_NrfTransmitReceive(TxData, RxData, sizeof(TxData));
}

static HAL_StatusTypeDef Cabell_NrfWriteBuffer(uint8_t Register,
                                               const uint8_t *Data,
                                               uint8_t Size)
{
  uint8_t Command = (uint8_t)(NRF_CMD_W_REGISTER | (Register & 0x1FU));
  HAL_StatusTypeDef Status;

  Cabell_CSN(GPIO_PIN_RESET);
  Status = HAL_SPI_Transmit(&hspi1, &Command, 1U, 2U);
  if (Status == HAL_OK)
  {
    Status = HAL_SPI_Transmit(&hspi1, (uint8_t *)Data, Size, 2U);
  }
  Cabell_CSN(GPIO_PIN_SET);

  return Status;
}

static HAL_StatusTypeDef Cabell_NrfReadStatus(uint8_t *StatusRegister)
{
  uint8_t Command = NRF_CMD_NOP;
  return Cabell_NrfTransmitReceive(&Command, StatusRegister, 1U);
}

static HAL_StatusTypeDef Cabell_NrfReadPayloadWidth(uint8_t *Width)
{
  uint8_t TxData[2] = {NRF_CMD_R_RX_PL_WID, NRF_CMD_NOP};
  uint8_t RxData[2];
  HAL_StatusTypeDef Status;

  Status = Cabell_NrfTransmitReceive(TxData, RxData, sizeof(TxData));
  if (Status == HAL_OK)
  {
    *Width = RxData[1];
  }
  return Status;
}

static HAL_StatusTypeDef Cabell_NrfReadPayload(uint8_t *Data, uint8_t Size)
{
  uint8_t Command = NRF_CMD_R_RX_PAYLOAD;
  HAL_StatusTypeDef Status;

  Cabell_CSN(GPIO_PIN_RESET);
  Status = HAL_SPI_Transmit(&hspi1, &Command, 1U, 2U);
  if (Status == HAL_OK)
  {
    Status = HAL_SPI_Receive(&hspi1, Data, Size, 2U);
  }
  Cabell_CSN(GPIO_PIN_SET);

  return Status;
}

static HAL_StatusTypeDef Cabell_NrfActivateFeatures(void)
{
  uint8_t TxData[2] = {NRF_CMD_ACTIVATE, 0x73U};
  uint8_t RxData[2];
  return Cabell_NrfTransmitReceive(TxData, RxData, sizeof(TxData));
}

static void Cabell_InitMicrosecondTimer(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  CabellCyclesPerMicrosecond = SystemCoreClock / 1000000U;
  if (CabellCyclesPerMicrosecond == 0U)
  {
    CabellCyclesPerMicrosecond = 1U;
  }

  CabellLastCycleCount = DWT->CYCCNT;
  CabellMicrosecondCount = 0U;
  CabellCycleRemainder = 0U;
}

static uint32_t Cabell_Micros(void)
{
  uint32_t NowCycle = DWT->CYCCNT;
  uint32_t DeltaCycle = NowCycle - CabellLastCycleCount;
  uint32_t TotalCycle = DeltaCycle + CabellCycleRemainder;

  CabellLastCycleCount = NowCycle;
  CabellMicrosecondCount += TotalCycle / CabellCyclesPerMicrosecond;
  CabellCycleRemainder = TotalCycle % CabellCyclesPerMicrosecond;

  return CabellMicrosecondCount;
}

static uint16_t Cabell_CalculateChecksum(const uint8_t *Data, uint16_t Size)
{
  uint16_t Checksum = 0x3C5AU;

  for (uint16_t Index = 0U; Index < Size; Index++)
  {
    Checksum = (uint16_t)((Checksum << 5) | (Checksum >> 11));
    Checksum = (uint16_t)(Checksum ^ Data[Index]);
  }

  return Checksum;
}

static uint8_t Cabell_LoadBinding(void)
{
  CabellFlashData_t FlashData;
  uint16_t Checksum;

  memcpy(&FlashData, (const void *)CABELL_FLASH_ADDRESS, sizeof(FlashData));
  Checksum = Cabell_CalculateChecksum((const uint8_t *)&FlashData,
                                      (uint16_t)offsetof(CabellFlashData_t, Checksum));

  if ((FlashData.Magic != CABELL_FLASH_MAGIC) ||
      (FlashData.Version != CABELL_FLASH_VERSION) ||
      (FlashData.Checksum != Checksum))
  {
    return 0U;
  }

  memcpy(CabellAddress, FlashData.Address, sizeof(CabellAddress));
  CabellData.ModelNumber = FlashData.ModelNumber;
  return 1U;
}

static HAL_StatusTypeDef Cabell_EraseBinding(void)
{
  FLASH_EraseInitTypeDef EraseInit = {0};
  uint32_t PageError = 0U;
  HAL_StatusTypeDef Status;

  Cabell_CE(GPIO_PIN_RESET);

  Status = HAL_FLASH_Unlock();
  if (Status != HAL_OK)
  {
    return Status;
  }

  EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInit.PageAddress = CABELL_FLASH_ADDRESS;
  EraseInit.NbPages = 1U;
  Status = HAL_FLASHEx_Erase(&EraseInit, &PageError);
  (void)HAL_FLASH_Lock();

  return Status;
}

static HAL_StatusTypeDef Cabell_SaveBinding(const uint8_t *Address, uint8_t ModelNumber)
{
  CabellFlashData_t FlashData = {0};
  const uint8_t *WriteData = (const uint8_t *)&FlashData;
  HAL_StatusTypeDef Status;

  FlashData.Magic = CABELL_FLASH_MAGIC;
  FlashData.Version = CABELL_FLASH_VERSION;
  memcpy(FlashData.Address, Address, sizeof(FlashData.Address));
  FlashData.ModelNumber = ModelNumber;
  FlashData.Checksum = Cabell_CalculateChecksum((const uint8_t *)&FlashData,
                                                (uint16_t)offsetof(CabellFlashData_t, Checksum));

  Status = Cabell_EraseBinding();
  if (Status != HAL_OK)
  {
    return Status;
  }

  Status = HAL_FLASH_Unlock();
  if (Status != HAL_OK)
  {
    return Status;
  }

  for (uint32_t Offset = 0U; Offset < sizeof(FlashData); Offset += 2U)
  {
    uint16_t HalfWord = WriteData[Offset];
    HalfWord |= (uint16_t)((uint16_t)WriteData[Offset + 1U] << 8);

    Status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                               CABELL_FLASH_ADDRESS + Offset,
                               HalfWord);
    if (Status != HAL_OK)
    {
      break;
    }
  }

  (void)HAL_FLASH_Lock();

  if ((Status == HAL_OK) &&
      (memcmp((const void *)CABELL_FLASH_ADDRESS, &FlashData, sizeof(FlashData)) != 0))
  {
    Status = HAL_ERROR;
  }

  return Status;
}

static uint64_t Cabell_AddressToValue(const uint8_t *Address)
{
  uint64_t Value = 0U;

  for (uint8_t Index = 0U; Index < 5U; Index++)
  {
    Value = (Value << 8) | Address[Index];
  }
  return Value;
}

static void Cabell_GetRadioSequence(uint8_t *Sequence, uint64_t Permutation)
{
  uint32_t Factorial = 1U;
  uint32_t Permutation32;

  for (uint8_t Index = 1U; Index <= CABELL_RADIO_SEQUENCE_SIZE; Index++)
  {
    Factorial *= Index;
    Sequence[Index - 1U] = Index - 1U;
  }

  Permutation32 = (uint32_t)(Permutation % Factorial);

  for (uint8_t Index = 0U; Index < CABELL_RADIO_SEQUENCE_SIZE; Index++)
  {
    uint32_t SelectedIndex;
    uint8_t SelectedValue;

    Factorial /= (uint32_t)(CABELL_RADIO_SEQUENCE_SIZE - Index);
    SelectedIndex = Index + (Permutation32 / Factorial);
    Permutation32 %= Factorial;
    SelectedValue = Sequence[SelectedIndex];

    while (SelectedIndex > Index)
    {
      Sequence[SelectedIndex] = Sequence[SelectedIndex - 1U];
      SelectedIndex--;
    }
    Sequence[Index] = SelectedValue;
  }
}

static uint8_t Cabell_GetNextRadioChannel(uint8_t PreviousChannel)
{
  uint8_t RelativeChannel;
  uint8_t CurrentBand;
  uint8_t NextBand;
  uint8_t PreviousSequenceValue;
  uint8_t PreviousSequencePosition = 0U;
  uint8_t NextSequencePosition;

  if ((PreviousChannel < CABELL_RADIO_MIN_CHANNEL) ||
      (PreviousChannel > CABELL_RADIO_MAX_CHANNEL))
  {
    PreviousChannel = CABELL_RADIO_MIN_CHANNEL;
  }

  RelativeChannel = PreviousChannel - CABELL_RADIO_MIN_CHANNEL;
  CurrentBand = RelativeChannel / CABELL_RADIO_SEQUENCE_SIZE;
  NextBand = (CurrentBand + 3U) % 5U;
  PreviousSequenceValue = RelativeChannel % CABELL_RADIO_SEQUENCE_SIZE;

  for (uint8_t Index = 0U; Index < CABELL_RADIO_SEQUENCE_SIZE; Index++)
  {
    if (CabellRadioSequence[Index] == PreviousSequenceValue)
    {
      PreviousSequencePosition = Index;
      break;
    }
  }

  NextSequencePosition = PreviousSequencePosition + 1U;
  if (NextSequencePosition >= CABELL_RADIO_SEQUENCE_SIZE)
  {
    NextSequencePosition = 0U;
  }

  return (uint8_t)((CABELL_RADIO_SEQUENCE_SIZE * NextBand) +
                   CabellRadioSequence[NextSequencePosition] +
                   CABELL_RADIO_MIN_CHANNEL);
}

static HAL_StatusTypeDef Cabell_SetRadioChannel(uint8_t Channel)
{
  HAL_StatusTypeDef Status;

  Cabell_CE(GPIO_PIN_RESET);
  Status = Cabell_NrfCommand(NRF_CMD_FLUSH_RX);
  if (Status == HAL_OK)
  {
    Status = Cabell_NrfWriteRegister(NRF_REG_STATUS, 0x70U);
  }
  if (Status == HAL_OK)
  {
    Status = Cabell_NrfWriteRegister(NRF_REG_RF_CH, Channel);
  }
  Cabell_CE(GPIO_PIN_SET);

  return Status;
}

static HAL_StatusTypeDef Cabell_SwitchToNextRadioChannel(void)
{
  CabellCurrentRadioChannel = Cabell_GetNextRadioChannel(CabellCurrentRadioChannel);
  return Cabell_SetRadioChannel(CabellCurrentRadioChannel);
}

static HAL_StatusTypeDef Cabell_ApplyRadioAddress(const uint8_t *Address)
{
  uint8_t NrfAddress[5];
  HAL_StatusTypeDef Status;

  for (uint8_t Index = 0U; Index < 5U; Index++)
  {
    NrfAddress[Index] = Address[4U - Index];
  }

  Cabell_CE(GPIO_PIN_RESET);
  Status = Cabell_NrfWriteBuffer(NRF_REG_RX_ADDR_P1, NrfAddress, sizeof(NrfAddress));
  if (Status != HAL_OK)
  {
    return Status;
  }

  Cabell_GetRadioSequence(CabellRadioSequence, Cabell_AddressToValue(Address));
  CabellCurrentRadioChannel = CABELL_RADIO_MIN_CHANNEL;
  CabellIsResyncing = 1U;
  CabellConsecutiveMissCount = 0U;
  CabellLastValidPacketUs = Cabell_Micros();

  Status = Cabell_SwitchToNextRadioChannel();
  CabellNextRadioSwitchUs = Cabell_Micros() + CABELL_RESYNC_CHANNEL_TIME_US;
  return Status;
}

static HAL_StatusTypeDef Cabell_NrfInit(const uint8_t *Address)
{
  uint8_t CheckValue;
  HAL_StatusTypeDef Status;

  Cabell_CE(GPIO_PIN_RESET);
  Cabell_CSN(GPIO_PIN_SET);
  HAL_Delay(5U);

  Status = Cabell_NrfWriteRegister(NRF_REG_CONFIG, 0x0CU);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_EN_AA, 0x00U);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_EN_RXADDR, 0x02U);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_SETUP_AW, 0x03U);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_SETUP_RETR, 0x00U);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_RF_SETUP, 0x26U);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_RX_PW_P1, 0x20U);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_FEATURE, 0x04U);
  if (Status != HAL_OK)
  {
    return Status;
  }

  Status = Cabell_NrfReadRegister(NRF_REG_FEATURE, &CheckValue);
  if ((Status == HAL_OK) && ((CheckValue & 0x04U) == 0U))
  {
    Status = Cabell_NrfActivateFeatures();
    if (Status == HAL_OK)
    {
      Status = Cabell_NrfWriteRegister(NRF_REG_FEATURE, 0x04U);
    }
  }
  if (Status == HAL_OK)
  {
    /* 部分兼容芯片执行ACTIVATE以前不会保存DYNPD，因此在这里统一重写 */
    Status = Cabell_NrfWriteRegister(NRF_REG_DYNPD, 0x02U);
  }
  if (Status != HAL_OK)
  {
    return Status;
  }

  Status = Cabell_NrfReadRegister(NRF_REG_FEATURE, &CheckValue);
  if ((Status != HAL_OK) || ((CheckValue & 0x04U) == 0U))
  {
    return HAL_ERROR;
  }

  /* 写入后读回地址宽度，用来判断NRF24L01是否真的存在 */
  Status = Cabell_NrfReadRegister(NRF_REG_SETUP_AW, &CheckValue);
  if ((Status != HAL_OK) || ((CheckValue & 0x03U) != 0x03U))
  {
    return HAL_ERROR;
  }

  Status = Cabell_NrfCommand(NRF_CMD_FLUSH_RX);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_STATUS, 0x70U);
  if (Status == HAL_OK) Status = Cabell_NrfWriteRegister(NRF_REG_CONFIG, 0x7FU);
  if (Status != HAL_OK)
  {
    return Status;
  }

  HAL_Delay(2U);
  return Cabell_ApplyRadioAddress(Address);
}

static void Cabell_SetSafeChannels(void)
{
  for (uint8_t Index = 0U; Index < CABELL_RECEIVER_CHANNEL_COUNT; Index++)
  {
    CabellData.Channel[Index] = CABELL_RECEIVER_CHANNEL_MID;
  }

  /* CH3是油门，CH5预留作电机使能，失联时都放到最低 */
  CabellData.Channel[2] = CABELL_RECEIVER_CHANNEL_MIN;
  CabellData.Channel[4] = CABELL_RECEIVER_CHANNEL_MIN;
}

static void Cabell_SetRadioError(void)
{
  CabellData.State = CABELL_RECEIVER_RADIO_ERROR;
  CabellData.IsConnected = 0U;
  CabellRadioReady = 0U;
  Cabell_SetSafeChannels();
}

static uint8_t Cabell_ValidatePacket(const uint8_t *Packet,
                                     uint8_t PacketSize,
                                     uint8_t *ChannelCount)
{
  uint8_t Mode = Packet[0] & 0x7FU;
  uint8_t ChannelReduction;
  uint8_t ExpectedPacketSize;
  uint8_t PayloadSize;
  uint16_t PacketChecksum;
  uint16_t ReceivedChecksum;

  if (PacketSize < CABELL_PACKET_HEADER_SIZE)
  {
    return 0U;
  }

  ChannelReduction = Packet[2] & 0x0FU;
  if (ChannelReduction > (CABELL_RECEIVER_CHANNEL_COUNT - CABELL_MIN_CHANNEL_COUNT))
  {
    ChannelReduction = CABELL_RECEIVER_CHANNEL_COUNT - CABELL_MIN_CHANNEL_COUNT;
  }

  ExpectedPacketSize = (uint8_t)(CABELL_PACKET_MAX_SIZE -
      ((((uint8_t)(ChannelReduction - (ChannelReduction % 2U))) / 2U) * 3U));
  if (PacketSize != ExpectedPacketSize)
  {
    return 0U;
  }

  PayloadSize = PacketSize - CABELL_PACKET_HEADER_SIZE;
  PacketChecksum = (uint16_t)Packet[3] + Packet[2] + Mode + Packet[1];
  for (uint8_t Index = 0U; Index < PayloadSize; Index++)
  {
    PacketChecksum = (uint16_t)(PacketChecksum + Packet[CABELL_PACKET_HEADER_SIZE + Index]);
  }

  ReceivedChecksum = (uint16_t)Packet[4] | ((uint16_t)Packet[5] << 8);
  if (PacketChecksum != ReceivedChecksum)
  {
    return 0U;
  }

  *ChannelCount = CABELL_RECEIVER_CHANNEL_COUNT - ChannelReduction;
  return 1U;
}

static uint8_t Cabell_DecodeChannels(const uint8_t *Packet,
                                     uint8_t ChannelCount,
                                     uint16_t *RawChannel)
{
  uint8_t PayloadIndex = 0U;

  for (uint8_t Channel = 0U; Channel < ChannelCount; Channel++)
  {
    uint16_t Value;

    Value = Packet[CABELL_PACKET_HEADER_SIZE + PayloadIndex];
    PayloadIndex++;
    Value |= (uint16_t)((uint16_t)Packet[CABELL_PACKET_HEADER_SIZE + PayloadIndex] << 8);

    if ((Channel % 2U) != 0U)
    {
      Value >>= 4;
      PayloadIndex++;
    }
    else
    {
      Value &= 0x0FFFU;
    }

    if ((Value < CABELL_RECEIVER_CHANNEL_MIN) ||
        (Value > CABELL_RECEIVER_CHANNEL_MAX))
    {
      return 0U;
    }
    RawChannel[Channel] = Value;
  }

  return 1U;
}

static void Cabell_CopyChannels(const uint16_t *RawChannel, uint8_t ChannelCount)
{
  uint16_t StandardChannel[CABELL_RECEIVER_CHANNEL_COUNT];

  for (uint8_t Index = 0U; Index < CABELL_RECEIVER_CHANNEL_COUNT; Index++)
  {
    StandardChannel[Index] = CABELL_RECEIVER_CHANNEL_MID;
  }

  /* Cabell包内前四路是EART，转换成遥控器页面看到的AETR */
  if (ChannelCount > 0U) StandardChannel[1] = RawChannel[0];
  if (ChannelCount > 1U) StandardChannel[0] = RawChannel[1];
  if (ChannelCount > 2U) StandardChannel[3] = RawChannel[2];
  if (ChannelCount > 3U) StandardChannel[2] = RawChannel[3];
  for (uint8_t Index = 4U; Index < ChannelCount; Index++)
  {
    StandardChannel[Index] = RawChannel[Index];
  }

  memcpy(CabellData.Channel, StandardChannel, sizeof(StandardChannel));
  CabellData.ChannelCount = ChannelCount;
}

static HAL_StatusTypeDef Cabell_EnterNormalMode(const uint8_t *Address, uint8_t ModelNumber)
{
  memcpy(CabellAddress, Address, sizeof(CabellAddress));
  CabellData.ModelNumber = ModelNumber;
  CabellIsBound = 1U;
  CabellData.IsConnected = 0U;
  CabellData.State = CABELL_RECEIVER_SEARCHING;
  Cabell_SetSafeChannels();
  return Cabell_ApplyRadioAddress(CabellAddress);
}

static void Cabell_ProcessPacket(uint8_t *Packet, uint8_t PacketSize, uint32_t NowUs)
{
  uint16_t RawChannel[CABELL_RECEIVER_CHANNEL_COUNT] = {0};
  uint8_t ChannelCount;
  uint8_t Mode;
  uint8_t PacketRadioChannel;

  if (PacketSize < CABELL_PACKET_HEADER_SIZE)
  {
    return;
  }

  /* 包头带有发射时所用信道，先按它跳到下一信道再做数据校验 */
  PacketRadioChannel = Packet[1] & 0x3FU;
  if ((PacketRadioChannel >= CABELL_RADIO_MIN_CHANNEL) &&
      (PacketRadioChannel <= CABELL_RADIO_MAX_CHANNEL))
  {
    CabellCurrentRadioChannel = PacketRadioChannel;
  }
  if (Cabell_SwitchToNextRadioChannel() != HAL_OK)
  {
    Cabell_SetRadioError();
    return;
  }
  CabellNextRadioSwitchUs = NowUs + CabellPacketPeriodUs + CABELL_PACKET_TIMEOUT_ADD_US;

  Packet[0] &= 0x7FU;
  Mode = Packet[0];
  if ((Cabell_ValidatePacket(Packet, PacketSize, &ChannelCount) == 0U) ||
      (Cabell_DecodeChannels(Packet, ChannelCount, RawChannel) == 0U))
  {
    return;
  }

  if ((CabellIsBound == 0U) &&
      ((Mode == CABELL_MODE_BIND) || (Mode == CABELL_MODE_BIND_NO_PULSE)) &&
      (ChannelCount == CABELL_RECEIVER_CHANNEL_COUNT))
  {
    uint8_t NewAddress[5];

    for (uint8_t Index = 0U; Index < 5U; Index++)
    {
      if (RawChannel[11U + Index] > (CABELL_RECEIVER_CHANNEL_MIN + 255U))
      {
        return;
      }
      NewAddress[Index] = (uint8_t)(RawChannel[11U + Index] - CABELL_RECEIVER_CHANNEL_MIN);
    }

    if (Cabell_SaveBinding(NewAddress, Packet[3]) == HAL_OK)
    {
      if (Cabell_EnterNormalMode(NewAddress, Packet[3]) != HAL_OK)
      {
        Cabell_SetRadioError();
      }
    }
    else
    {
      CabellData.State = CABELL_RECEIVER_FLASH_ERROR;
    }
    return;
  }

  if ((CabellIsBound == 0U) || (Packet[3] != CabellData.ModelNumber))
  {
    return;
  }

  if (Mode == CABELL_MODE_UNBIND)
  {
    if (CabellReceiver_Unbind() != HAL_OK)
    {
      CabellData.State = CABELL_RECEIVER_FLASH_ERROR;
      CabellData.IsConnected = 0U;
      Cabell_SetSafeChannels();
    }
    return;
  }

  if ((Mode != CABELL_MODE_NORMAL) &&
      (Mode != CABELL_MODE_NORMAL_TELEMETRY) &&
      (Mode != CABELL_MODE_SET_FAILSAFE))
  {
    return;
  }

  CabellPacketPeriodUs = CABELL_PACKET_PERIOD_US;
  if ((Mode == CABELL_MODE_NORMAL_TELEMETRY) && (ChannelCount > 6U))
  {
    uint8_t AddedChannel = ChannelCount - 6U;
    if (AddedChannel > 10U) AddedChannel = 10U;
    CabellPacketPeriodUs += (uint32_t)AddedChannel * 100U;
  }
  CabellNextRadioSwitchUs = NowUs + CabellPacketPeriodUs + CABELL_PACKET_TIMEOUT_ADD_US;

  Cabell_CopyChannels(RawChannel, ChannelCount);
  CabellData.PacketCount++;
  CabellData.LastPacketTick = HAL_GetTick();
  CabellData.IsConnected = 1U;
  CabellData.State = CABELL_RECEIVER_CONNECTED;
  CabellLastValidPacketUs = NowUs;
  CabellConsecutiveMissCount = 0U;
  CabellIsResyncing = 0U;
}

static void Cabell_CheckFailsafe(uint32_t NowTick)
{
  if ((CabellData.IsConnected != 0U) &&
      ((uint32_t)(NowTick - CabellData.LastPacketTick) >= CABELL_RECEIVER_FAILSAFE_TIME_MS))
  {
    CabellData.IsConnected = 0U;
    CabellData.State = CABELL_RECEIVER_FAILSAFE;
    Cabell_SetSafeChannels();
  }
}

HAL_StatusTypeDef CabellReceiver_Init(void)
{
  const uint8_t *StartupAddress;
  HAL_StatusTypeDef Status;

  memset(&CabellData, 0, sizeof(CabellData));
  Cabell_SetSafeChannels();
  CabellData.State = CABELL_RECEIVER_NOT_INITIALIZED;
  CabellPacketPeriodUs = CABELL_PACKET_PERIOD_US;
  Cabell_InitMicrosecondTimer();

  CabellIsBound = Cabell_LoadBinding();
  if (CabellIsBound != 0U)
  {
    StartupAddress = CabellAddress;
    CabellData.State = CABELL_RECEIVER_SEARCHING;
  }
  else
  {
    StartupAddress = CabellBindAddress;
    CabellData.State = CABELL_RECEIVER_BINDING;
  }

  Status = Cabell_NrfInit(StartupAddress);
  if (Status != HAL_OK)
  {
    Cabell_SetRadioError();
    return Status;
  }

  CabellRadioReady = 1U;
  return HAL_OK;
}

void CabellReceiver_Process(void)
{
  uint8_t StatusRegister;
  uint8_t FifoStatus;
  uint8_t PacketSize;
  uint8_t Packet[CABELL_PACKET_MAX_SIZE];
  uint32_t NowUs;

  if (CabellRadioReady == 0U)
  {
    return;
  }

  NowUs = Cabell_Micros();
  Cabell_CheckFailsafe(HAL_GetTick());

  if ((Cabell_NrfReadStatus(&StatusRegister) != HAL_OK) ||
      (Cabell_NrfReadRegister(NRF_REG_FIFO_STATUS, &FifoStatus) != HAL_OK))
  {
    Cabell_SetRadioError();
    return;
  }

  if (((StatusRegister & NRF_STATUS_RX_DR) != 0U) ||
      ((FifoStatus & NRF_FIFO_RX_EMPTY) == 0U))
  {
    if ((Cabell_NrfReadPayloadWidth(&PacketSize) != HAL_OK) ||
        (PacketSize == 0U) || (PacketSize > CABELL_PACKET_MAX_SIZE))
    {
      (void)Cabell_NrfCommand(NRF_CMD_FLUSH_RX);
      (void)Cabell_NrfWriteRegister(NRF_REG_STATUS, NRF_STATUS_RX_DR);
      return;
    }

    if (Cabell_NrfReadPayload(Packet, PacketSize) != HAL_OK)
    {
      Cabell_SetRadioError();
      return;
    }
    (void)Cabell_NrfWriteRegister(NRF_REG_STATUS, NRF_STATUS_RX_DR);
    Cabell_ProcessPacket(Packet, PacketSize, NowUs);
    return;
  }

  if ((int32_t)(NowUs - CabellNextRadioSwitchUs) >= 0)
  {
    if (CabellIsResyncing != 0U)
    {
      if (Cabell_SwitchToNextRadioChannel() != HAL_OK)
      {
        Cabell_SetRadioError();
        return;
      }
      CabellNextRadioSwitchUs = NowUs + CABELL_RESYNC_CHANNEL_TIME_US;
    }
    else
    {
      uint8_t SwitchCount = 0U;

      do
      {
        if (Cabell_SwitchToNextRadioChannel() != HAL_OK)
        {
          Cabell_SetRadioError();
          return;
        }
        CabellNextRadioSwitchUs += CabellPacketPeriodUs;
        CabellConsecutiveMissCount++;
        SwitchCount++;
      }
      while (((int32_t)(NowUs - CabellNextRadioSwitchUs) >= 0) &&
             (SwitchCount < CABELL_MAX_FAST_SWITCH_COUNT));

      if (((uint32_t)(NowUs - CabellLastValidPacketUs) >= CABELL_RESYNC_TIMEOUT_US) ||
          (CabellConsecutiveMissCount > 5U))
      {
        CabellIsResyncing = 1U;
        CabellConsecutiveMissCount = 0U;
        CabellPacketPeriodUs = CABELL_PACKET_PERIOD_US;
        CabellNextRadioSwitchUs = NowUs + CABELL_RESYNC_CHANNEL_TIME_US;
      }
    }
  }
}

void CabellReceiver_GetData(CabellReceiver_Data_t *Data)
{
  uint32_t Primask;

  if (Data == NULL)
  {
    return;
  }

  Primask = __get_PRIMASK();
  __disable_irq();
  memcpy(Data, &CabellData, sizeof(*Data));
  if (Primask == 0U)
  {
    __enable_irq();
  }
}

HAL_StatusTypeDef CabellReceiver_Unbind(void)
{
  HAL_StatusTypeDef Status;

  Status = Cabell_EraseBinding();
  if (Status != HAL_OK)
  {
    CabellData.State = CABELL_RECEIVER_FLASH_ERROR;
    CabellData.IsConnected = 0U;
    Cabell_SetSafeChannels();
    return Status;
  }

  CabellIsBound = 0U;
  CabellData.ModelNumber = 0U;
  CabellData.IsConnected = 0U;
  CabellData.State = CABELL_RECEIVER_BINDING;
  Cabell_SetSafeChannels();

  Status = Cabell_ApplyRadioAddress(CabellBindAddress);
  if (Status != HAL_OK)
  {
    Cabell_SetRadioError();
  }
  return Status;
}
