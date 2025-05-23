/*
  MCU specific utility functions for the bootloader
 */
/*
  based on https://github.com/AlkaMotors/AM32_Bootloader_F051/blob/main/Core/
 */
#pragma once

#include <stm32g4xx_ll_rcc.h>

/*
  64k ram
 */
#define RAM_BASE 0x20000000
#define RAM_SIZE 32*1024

/*
  we have up to 512k of flash, but only use 64k for now
 */
#define BOARD_FLASH_SIZE 64

#define GPIO_PIN(n) (1U<<(n))

#define GPIO_PULL_NONE LL_GPIO_PULL_NO
#define GPIO_PULL_UP   LL_GPIO_PULL_UP
#define GPIO_PULL_DOWN LL_GPIO_PULL_DOWN

#define GPIO_OUTPUT_PUSH_PULL LL_GPIO_OUTPUT_PUSHPULL

static inline void gpio_mode_set_input(uint32_t pin, uint32_t pull_up_down)
{
  LL_GPIO_SetPinMode(input_port, pin, LL_GPIO_MODE_INPUT);
  LL_GPIO_SetPinPull(input_port, pin, pull_up_down);
}

static inline void gpio_mode_set_output(uint32_t pin, uint32_t output_mode)
{
  LL_GPIO_SetPinMode(input_port, pin, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinOutputType(input_port, pin, output_mode);
}

static inline void gpio_set(uint32_t pin)
{
  LL_GPIO_SetOutputPin(input_port, pin);
}

static inline void gpio_clear(uint32_t pin)
{
  LL_GPIO_ResetOutputPin(input_port, pin);
}

static inline bool gpio_read(uint32_t pin)
{
  return LL_GPIO_IsInputPinSet(input_port, pin);
}

#define BL_TIMER TIM2

/*
  initialise timer for 1us per tick
 */
static inline void bl_timer_init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

  TIM_InitStruct.Prescaler = (160-1); // HSI PLL clock is 160Mz, want 1MHz timer
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 0xFFFFFFFF;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  LL_TIM_Init(BL_TIMER, &TIM_InitStruct);
  LL_TIM_DisableARRPreload(BL_TIMER);
  LL_TIM_SetClockSource(BL_TIMER, LL_TIM_CLOCKSOURCE_INTERNAL);
  LL_TIM_SetTriggerOutput(BL_TIMER, LL_TIM_TRGO_RESET);
  LL_TIM_DisableMasterSlaveMode(BL_TIMER);

  LL_TIM_SetCounterMode(BL_TIMER, LL_TIM_COUNTERMODE_UP);
  LL_TIM_EnableCounter(BL_TIMER);
}

/*
  disable timer ready for app start
 */
static inline void bl_timer_disable(void)
{
  LL_TIM_DeInit(BL_TIMER);
}

static inline uint16_t bl_timer_us(void)
{
  return LL_TIM_GetCounter(BL_TIMER);
}

/*
  initialise clocks
 */
static inline void bl_clock_config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
  while (LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_4) ;
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  while (LL_PWR_IsActiveFlag_VOS() != 0) ;

  LL_RCC_HSI_Enable();

  /* Wait till HSI is ready */
  while (LL_RCC_HSI_IsReady() != 1) ;

  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_2, 40, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_EnableDomain_SYS();
  LL_RCC_PLL_Enable();

  /* Wait till PLL is ready */
  while (LL_RCC_PLL_IsReady() != 1) ;
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

  /* Wait till System clock is ready */
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) ;

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
}

static inline void bl_gpio_init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

  GPIO_InitStruct.Pin = input_pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;

  LL_GPIO_Init(input_port, &GPIO_InitStruct);
}

/*
  return true if the MCU booted under a software reset
 */
static inline bool bl_was_software_reset(void)
{
  return (RCC->CSR & RCC_CSR_SFTRSTF) != 0;
}

void Error_Handler()
{
  while (1) {}
}

/*
  jump from the bootloader to the application code
 */
static inline void jump_to_application(void)
{
  __disable_irq();
  bl_timer_disable();
  const uint32_t app_address = MCU_FLASH_START + FIRMWARE_RELATIVE_START;
  const uint32_t *app_data = (const uint32_t *)app_address;
  const uint32_t stack_top = app_data[0];
  const uint32_t JumpAddress = app_data[1];

  // setup vector table
  SCB->VTOR = app_address;

  // setup sp, msp and jump
  asm volatile(
    "mov sp, %0	\n"
    "msr msp, %0	\n"
    "bx	%1	\n"
    : : "r"(stack_top), "r"(JumpAddress) :);
}

/*
  nothing to do in SystemInit()
 */
void SystemInit()
{
}
