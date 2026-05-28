#include "main.h"
#include "usb_device.h"
#include <stdio.h>      
#include <string.h>
#include <stm32f1xx.h>

TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;

uint8_t uart_buf[1];
uint8_t ticks;  

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_ADC1_BareMetal_Init(void); 
void delay_us(uint32_t us);

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

int _write(int fd, char * ptr, int len)
{
  HAL_UART_Transmit(&huart1, (uint8_t *) ptr, len, HAL_MAX_DELAY);
  return len;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(uart_buf[0] != '?' ) {
    uart_buf[0]++;  
    HAL_UART_Transmit(&huart1, uart_buf, 1, 10);
  } else {
    printf("Sw Version %d.%d\r\n", SW_VERSION/10, SW_VERSION%10);
  }
  HAL_UART_Receive_IT(&huart1, uart_buf, 1); 
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM2) {
    if(ticks++ == 100) ticks = 0;
  }
}

void delay_us(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 2; i++) {
        __NOP();
    }
}

static void MX_ADC1_BareMetal_Init(void) {
    GPIOA->CRL &= ~(0xF << (0 * 4));     // PA0 rămâne pentru microfon
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;  
    ADC1->CR2 |= ADC_CR2_ADON;           
    
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL);
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL);
}

uint16_t read_jack_baremetal(void) {
    ADC1->SQR3 = 0;                   
    ADC1->CR2 |= ADC_CR2_ADON;        
    uint32_t timeout = 10000;
    while (!(ADC1->SR & ADC_SR_EOC) && timeout--) { __NOP(); } 
    return ADC1->DR;                  
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM2_Init();
  MX_ADC1_BareMetal_Init(); 

  HAL_UART_Receive_IT(&huart1, uart_buf, 1); 
  HAL_TIM_Base_Start_IT(&htim2);  

  /* --- ORDINEA EXACTĂ A PINILOR TĂI (DE JOS IN SUS) --- */
  GPIO_TypeDef* ports[] = {
      GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOA, GPIOA, GPIOA, GPIOA
  };
  uint16_t pins[] = {
      GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15, // LED 1 - 6
      GPIO_PIN_8,  GPIO_PIN_1,  GPIO_PIN_2,  GPIO_PIN_3                               // LED 7 - 10 (A8, A1, A2, A3)
  };

  /* --- TEST HARDWARE LA PORNIRE --- */
  for (int t = 0; t < 3; t++) {
      for(int i=0; i<10; i++) HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_SET);
      HAL_Delay(200);
      for(int i=0; i<10; i++) HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_RESET);
      HAL_Delay(200);
  }

  /* --- AUTO-CALIBRARE DINAMICĂ --- */
  uint32_t bias_sum = 0;
  for(int c = 0; c < 200; c++) {
      bias_sum += read_jack_baremetal();
      HAL_Delay(2);
  }
  int BIAS = bias_sum / 200;
  if(BIAS < 500 || BIAS > 3500) BIAS = 2048; 

  int curent_volume = 0;

  while (1)
  {
    int max_sample = 0;

    for (int s = 0; s < 200; s++) {
        int sample = read_jack_baremetal();
        int amplitude = sample - BIAS;
        if (amplitude < 0) amplitude = -amplitude;
        if (amplitude > max_sample) max_sample = amplitude;
    }

    int target_volume = max_sample / 60; // Filtru sensibilitate medie (60)
    
    if (target_volume <= 1) {
        target_volume = 0;
    }
    
    if (target_volume > 10) target_volume = 10;

    if (curent_volume < target_volume) {
        curent_volume = target_volume; 
    } else if (curent_volume > target_volume) {
        curent_volume--; 
    }

    for (int i = 0; i < 10; i++) {
        if (i < curent_volume) {
            HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_SET);   
        } else {
            HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_RESET); 
        }
    }

    if( HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET ) {     
        HAL_Delay(25);                                                 
        if( HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET ) { 
          while( HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET); 
          printf("USER Button Pressed\n\r");
        }  
    }

    delay_us(6000); 
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 100;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim2);
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_8, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
