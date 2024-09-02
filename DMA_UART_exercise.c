#include "COTS/MCAL/RCC/RCC_interface.h"
#include "COTS/MCAL/GPIO/GPIO_interface.h"
#include "COTS/MCAL/NVIC/NVIC_Interface.h"
#include "COTS/MCAL/UART/UART_interface.h"
#include "COTS/MCAL/DMA/DMA_interface.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "string.h"

#define MAX_EVENTS 5 // specify the maximum number of not processing events (Rx data)

void SysTick_callback();
void DMA_TX_CompleteInterrupt_callback();
void DMA_RX_CompleteInterrupt_callback();

uint8 Error_Code;
volatile uint16 gADC_data;

uint8 g_UART_data[42] = "hi from Master ECU this msg is encrypted\n"; // 42 byte of data
uint8 RxData_arr_of_ptrs[MAX_EVENTS][5];

TaskHandle_t vEncryptTxTask = NULL;
TaskHandle_t vDecryptRxTask = NULL;

SemaphoreHandle_t xReceiveCountingSemaphore = NULL;
SemaphoreHandle_t xTransmitBinarySemaphore = NULL;
/***Task prototype****/
void vEncryptTxTaskHandler(void *parms);
void vDecryptRxTaskHandler(void *parms);

int main(void)
{
    RCC_initConfig_t RCC_init;
    RCC_init.ADCPRE = ADCPRE_PCLK2_2; // ADC prescaler
    RCC_init.HPRE = HPRE_NO_DIV;      // AHB prescaler
    RCC_init.PPRE1 = PPRE1_NO_DIV;    // APB1 prescaler
    RCC_init.PPRE2 = PPRE2_NO_DIV;    // APB2 prescaler
    RCC_init.RCC_CLK_Type = RCC_HSI;
    MRCC_voidInitSysClock(&RCC_init);
    MRCC_voidEnableClock(RCC_APB2, PERIPHERAL_EN_IOPA);
    MRCC_voidEnableClock(RCC_AHB, _PERIPHERAL_EN_DMA1EN);
    MGPIO_VoidSetPinMode_TYPE(_GPIOA_PORT, pin2, OUTPUT_SPEED_10MHZ_PUSHPULL);
    MUSART1_voidInit();
    MUSART1_voidEnable();
    /*Enable the transmission and reception by DMA*/
    MUSART1_VoidEnableDMATransmission();
    MUSART1_VoidEnableDMAReception();

    // MSYSTICK_VoidStartSYSTICK(1000000,SysTick_callback);

    /*Enable DMA interrupt*/
    MNVIC_VoidSetPriorityPolicy(Group16Sub0);
    MNVIC_VoidSetPriority(DMA1_Channel4_IRQ, 1, 0);
    MNVIC_VoidEnableInterrupt(DMA1_Channel4_IRQ);
    MDMA_VoidEnableInterrupt(3, DMA_INTERRUPT_COMPLETE_TRANSMISSION, DMA_TX_CompleteInterrupt_callback); // ch4

    MNVIC_VoidSetPriority(DMA1_Channel5_IRQ, 1, 0);
    MNVIC_VoidEnableInterrupt(DMA1_Channel5_IRQ);
    MDMA_VoidEnableInterrupt(4, DMA_INTERRUPT_COMPLETE_TRANSMISSION, DMA_RX_CompleteInterrupt_callback); // ch5

    /******tasks creation******/
    xTaskCreate(vEncryptTxTaskHandler, "EncryptTx", 128, NULL, 1, vEncryptTxTask);
    /*RX task has higher priority to start immidatly after return from ISR to handle the received msg*/
    xTaskCreate(vDecryptRxTaskHandler, "DecryptRx", 128, NULL, 2, vDecryptRxTask);

    /*create semphores for sync*/
    /*counting semaphore between ISR and RXTask*/
    /*Max number of events and initial value is 0 to increment with events*/
    xReceiveCountingSemaphore = xSemaphoreCreateCounting(MAX_EVENTS, 0);
    /*binary semaphore between RXTask and RX Task*/
    xTransmitBinarySemaphore = xSemaphoreCreateBinary();

    vTaskStartScheduler();

    while (1)
    {
    }

    return 0;
}

void DMA_TX_CompleteInterrupt_callback()
{
    MDMA_VoidDisableChannel(DMA1_Channel4_IRQ);
    MUSART1_VoidDisableDMATransmission();
}
void DMA_RX_CompleteInterrupt_callback()
{
    static uint8 c = 0;
    MDMA_VoidDisableChannel(DMA1_Channel5_IRQ);
    MUSART1_VoidDisableDMAReception();

    /* ISR of the receiving data event*/
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdTRUE;
    /*give the counting semphore to wake the processing RX task*/
    xSemaphoreGiveFromISR(xReceiveCountingSemaphore, &xHigherPriorityTaskWoken);
    /*hi from another ECU this msg is encrypted*/
    MUSART1_voidSendData('\n');
    MUSART1_voidSendString(RxData_arr_of_ptrs[c % 5]);
    MUSART1_voidSendData('\n');
    c++;
}

void vEncryptTxTaskHandler(void *parms)
{
    /*DMA Transmission initialization*/
    DMA_InitTypeDef DMA_init;
    DMA_init.Peripheral = DMA_PERIPHERAL_USART1_TX;
    DMA_init.DMA_Peripheral_address = (uint32 *)0x40013804; // TX Data
    uint32 *ptr = &g_UART_data;
    DMA_init.DMA_Memory_address = ptr;
    DMA_init.DMA_Data_Number = 42;
    DMA_init.DMA_Direction = DMA_DIRECTION_READ_FROM_MEMORY;
    DMA_init.DMA_Mem2MemMode = DMA_DISABLE;
    DMA_init.DMA_PERIPHERAL_Data_Size = DMA_SIZE_8_BIT;
    DMA_init.DMA_MEMORY_Data_Size = DMA_SIZE_8_BIT;
    DMA_init.DMA_MEMORY_PTR_INC = DMA_ENABLE;
    DMA_init.DMA_PERIPHERAL_PTR_INC = DMA_DISABLE;
    DMA_init.DMA_CircularMode = DMA_ENABLE;
    uint8 transmit_bool = 1;
    /*ticks of 500ms then leave the blocking state*/
    const TickType_t xMaxExpectedBlockTime = pdMS_TO_TICKS(1000);
    while (1)
    {
        if (transmit_bool)
        {
            /*encrypt data in g_UART_data*/
            /** TODO: implement encryption algorithm */
            /********************************************** */
            /*start the DMA tranmission*/
            MDMA_VoidChannelInit(&DMA_init);
            MUSART1_VoidEnableDMATransmission();
        }

        /*wait for event from RX task to start the next transmission*/

        if (xSemaphoreTake(xTransmitBinarySemaphore, xMaxExpectedBlockTime) == pdTRUE)
        {
            transmit_bool = 1;
        }
        else
        {
            transmit_bool = 0;
        }
    }
}
void vDecryptRxTaskHandler(void *parms)
{
    /*DMA Reception initialization*/
    DMA_InitTypeDef DMA_RX_init;
    DMA_RX_init.Peripheral = DMA_PERIPHERAL_USART1_RX;
    DMA_RX_init.DMA_Peripheral_address = (uint32 *)0x40013804; // DR register
    uint32 *ptr2 = &RxData_arr_of_ptrs[0];
    DMA_RX_init.DMA_Memory_address = ptr2;
    DMA_RX_init.DMA_Data_Number = 5;
    DMA_RX_init.DMA_Direction = DMA_DIRECTION_READ_FROM_PERIPHERAL;
    DMA_RX_init.DMA_Mem2MemMode = DMA_DISABLE;
    DMA_RX_init.DMA_PERIPHERAL_Data_Size = DMA_SIZE_8_BIT;
    DMA_RX_init.DMA_MEMORY_Data_Size = DMA_SIZE_8_BIT;
    DMA_RX_init.DMA_MEMORY_PTR_INC = DMA_ENABLE;
    DMA_RX_init.DMA_PERIPHERAL_PTR_INC = DMA_DISABLE;
    DMA_RX_init.DMA_CircularMode = DMA_ENABLE;
    MDMA_VoidChannelInit(&DMA_RX_init);
    MUSART1_VoidEnableDMAReception();
    static uint8 count = 0;
    /*ticks of 500ms then leave the blocking state*/
    const TickType_t xMaxExpectedBlockTime = pdMS_TO_TICKS(1000);
    while (1)
    {
        /*wait for the reciving event*/
        if (xSemaphoreTake(xReceiveCountingSemaphore, xMaxExpectedBlockTime) == pdTRUE)
        {
            /*Decrypt Data received in RX Data*/
            /** TODO: implement decryption algorithm */
            /********************************************** */
            
            /*update the data to be sent next*/
            g_UART_data[40] = count + '0';
            count++;
            if (count % 5 == 0)
                memset((uint8 *)RxData_arr_of_ptrs, '\0', MAX_EVENTS * 5 * sizeof(char));
            DMA_RX_init.DMA_Memory_address = &RxData_arr_of_ptrs[count % 5];
            DMA_RX_init.DMA_Data_Number = 5;
            MDMA_VoidChannelInit(&DMA_RX_init);
            MUSART1_VoidEnableDMAReception();
            /*wake the TX task*/
            while (xSemaphoreGive(xTransmitBinarySemaphore) != pdPASS)
                ;
        }
    }
}
