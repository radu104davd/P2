#include <stdint.h>
#include "main.h"

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart1;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
uint16_t Read_ADC(void);
void Error_Handler(void);

#define NUM_LEDS 10

GPIO_TypeDef* ports[NUM_LEDS] = {
    GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB,
    GPIOA, GPIOA, GPIOA, GPIOA
};

uint16_t pins[NUM_LEDS] = {
    GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12,
    GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15,
    GPIO_PIN_4, GPIO_PIN_3, GPIO_PIN_2, GPIO_PIN_1
};

uint16_t adc_value = 0;
uint16_t audio_center = 2048;
uint16_t amplitude = 0;

uint8_t led_level = 0;
uint8_t peak_level = 0;

uint32_t hold_time = 1000;
uint32_t last_peak_time = 0;

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();

    for (uint8_t k = 0; k < 3; k++)
    {
        for (uint8_t i = 0; i < NUM_LEDS; i++)
        {
            HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_SET);
        }

        HAL_Delay(300);

        for (uint8_t i = 0; i < NUM_LEDS; i++)
        {
            HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_RESET);
        }

        HAL_Delay(300);
    }

    while (1)
    {
        adc_value = Read_ADC();

        if (adc_value > audio_center)
        {
            amplitude = adc_value - audio_center;
        }
        else
        {
            amplitude = audio_center - adc_value;
        }

        led_level = (amplitude * NUM_LEDS) / 700;

        if (led_level > NUM_LEDS)
        {
            led_level = NUM_LEDS;
        }

        if (led_level > peak_level)
        {
            peak_level = led_level;
            last_peak_time = HAL_GetTick();
        }

        if ((HAL_GetTick() - last_peak_time) > hold_time)
        {
            peak_level = led_level;
            last_peak_time = HAL_GetTick();
        }

        for (uint8_t i = 0; i < NUM_LEDS; i++)
        {
            if ((i < peak_level) || (peak_level > 0 && i == peak_level-1))
            {
                HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_SET);
            }
            else
            {
                HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_RESET);
            }
        }

        HAL_Delay(10);
    }
}

uint16_t Read_ADC(void)
{
    uint16_t value = 0;

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        value = HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);

    return value;
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
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

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB,
                      GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                      GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOA,
                      GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4,
                      GPIO_PIN_RESET);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                          GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
        for (uint8_t i = 0; i < NUM_LEDS; i++)
        {
            HAL_GPIO_WritePin(ports[i], pins[i], GPIO_PIN_SET);
        }

        HAL_Delay(100);
    }
}