/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "memorymap.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

uint16_t foxX = 20;        // Fixed X position for fox
uint16_t foxY =65;        // Y position (ground position)
int8_t foxVelocityY = 0;   // Vertical velocity (for jumping)
uint8_t isJumping = 0;     // Jump state
uint16_t groundLevel = 75; // Ground level position

// Background variables
uint16_t bgOffset = 0;     // Background scroll offset
uint8_t treePositions[5] = {20, 60, 100, 150, 200}; // X positions of trees
uint8_t scrollSpeed = 2;   // Pixels per frame to scroll
uint32_t lastUpdateTime = 0; // For timing control

#define MAX_VISIBLE_OBSTACLES 20  // Increased buffer size
uint16_t obstaclePosX[MAX_VISIBLE_OBSTACLES]; // X positions of obstacles
uint8_t obstacleActive[MAX_VISIBLE_OBSTACLES]; // Whether each obstacle is active
uint32_t lastObstacleTime = 0; // Time when last obstacle was spawned
uint32_t obstacleInterval = 1500; // Spawn interval in milliseconds (1.5 seconds)
uint8_t nextObstacleIndex = 0; // Index for circular buffer

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

	/* Configure the MPU attributes for the QSPI 256MB without instruction access */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress      = QSPI_BASE;
  MPU_InitStruct.Size             = MPU_REGION_SIZE_256MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the MPU attributes for the QSPI 8MB (QSPI Flash Size) to Cacheable WT */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress      = QSPI_BASE;
  MPU_InitStruct.Size             = MPU_REGION_SIZE_8MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RO;
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

static void LED_Blink(uint32_t Hdelay,uint32_t Ldelay)
{
	HAL_GPIO_WritePin(E3_GPIO_Port,E3_Pin,GPIO_PIN_SET);
	HAL_Delay(Hdelay - 1);
	HAL_GPIO_WritePin(E3_GPIO_Port,E3_Pin,GPIO_PIN_RESET);
	HAL_Delay(Ldelay-1);
}

/**
  * @brief  Get the current time and date.
  * @param
  * @retval None
  */
static void RTC_CalendarShow(RTC_DateTypeDef *sdatestructureget,RTC_TimeTypeDef *stimestructureget)
{
  /* ����ͬʱ��ȡʱ������� ��Ȼ�ᵼ���´�RTC���ܶ�ȡ */
  /* Both time and date must be obtained or RTC cannot be read next time */
  /* Get the RTC current Time */
  HAL_RTC_GetTime(&hrtc, stimestructureget, RTC_FORMAT_BIN);
  /* Get the RTC current Date */
  HAL_RTC_GetDate(&hrtc, sdatestructureget, RTC_FORMAT_BIN);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  #ifdef W25Qxx
    SCB->VTOR = QSPI_BASE;
  #endif
  MPU_Config();
  CPU_CACHE_Enable();

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RTC_Init();
  MX_SPI4_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
//	HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
//	__HAL_TIM_SetCompare(&htim1,TIM_CHANNEL_2,10);
	LCD_Test();
  InitScene();
  InitObstacles();
    
  bgOffset = 0;
  scrollSpeed = 2; // Ensure this is not zero
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    UpdatePhysics();
        
    // Force update background (ensure it's moving)
    ForceUpdateBackground();
    UpdateObstacles(); // Update obstacles
    
    // Update scene with new positions
    UpdateScene();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void DrawFox(uint16_t x, uint16_t y) {
  // Main body
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x, y, 16, 10, BROWN);
  
  // Head
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 10, y - 6, 10, 10, BROWN);
  
  // Eyes
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 14, y - 4, 2, 2, WHITE);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 18, y - 4, 2, 2, WHITE);
  
  // Ears
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 10, y - 10, 3, 4, BROWN);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 17, y - 10, 3, 4, BROWN);
  
  // Tail
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x - 5, y + 2, 5, 4, BROWN);
  
  // Legs (attached to body's current position)
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 2, y + 10, 2, 5, BROWN);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 12, y + 10, 2, 5, BROWN);
}

void DrawTree(uint16_t x, uint16_t y) {
  // Tree trunk
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x - 1, y - 20, 3, 20, BROWN);
  
  // Tree foliage (simple triangle shape made of rectangles)
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x - 6, y - 30, 13, 5, GREEN);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x - 5, y - 35, 11, 5, GREEN);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x - 4, y - 40, 9, 5, GREEN);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x - 3, y - 45, 7, 5, GREEN);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x - 2, y - 50, 5, 5, GREEN);
}

// Draw the scene (background, ground, fox, text)
void DrawScene(void) {
  uint8_t text[20];
  uint8_t i;
  
  // Clear the screen
  ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, ST7735Ctx.Width, ST7735Ctx.Height, LIGHTBLUE); // Sky
  
  // Draw scrolling trees
  for (i = 0; i < 5; i++) {
      // Calculate actual X position with scroll offset
      int16_t treeX = (treePositions[i] - bgOffset) % ST7735Ctx.Width;
      
      if (treeX < 0) {
          treeX += ST7735Ctx.Width;
      }
      
      DrawTree(treeX, groundLevel);
  }
  
  ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, groundLevel, ST7735Ctx.Width, ST7735Ctx.Height - groundLevel, GREEN); // Grassy ground
  ST7735_LCD_Driver.DrawHLine(&st7735_pObj, 0, groundLevel, ST7735Ctx.Width, BROWN); // Ground line
  
  // Draw the fox at its fixed position (but varying height if jumping)
  DrawFox(foxX, foxY);
  
}

void UpdateBackground(void) {
  // Increase background offset
  bgOffset += scrollSpeed;
  
  // Handle wrapping (if we've scrolled past the screen width)
  if (bgOffset >= ST7735Ctx.Width) {
      bgOffset = 0;
  }
}

void ForceUpdateBackground(void) {
  // Always update the background offset
  bgOffset += scrollSpeed;
  
  // Create a very small delay to ensure timing is consistent
  HAL_Delay(5);
}

void UpdateScene(void) {
  uint8_t i;
  
  // Always clear the entire sky area
  ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, ST7735Ctx.Width, groundLevel, LIGHTBLUE);
  
  // Draw trees in their new positions
  for (i = 0; i < 5; i++) {
    int16_t treeX = (treePositions[i] - bgOffset) % ST7735Ctx.Width;
    if (treeX < 0) {
      treeX += ST7735Ctx.Width;
    }
    DrawTree(treeX, groundLevel);
  }
  
  // Redraw the ground
  ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, groundLevel, ST7735Ctx.Width, 
                           ST7735Ctx.Height - groundLevel, GREEN);
  ST7735_LCD_Driver.DrawHLine(&st7735_pObj, 0, groundLevel, ST7735Ctx.Width, BROWN);
  
  // Draw obstacles
  DrawObstacles();
  
  // Draw fox with current position
  DrawFox(foxX, foxY);
}

// Check input and start jump if needed
void CheckInput(void) {
  // Check if KEY button is pressed and fox is not already jumping
  if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET && !isJumping) {
      isJumping = 1;
      foxVelocityY = -8; // Negative velocity for upward movement
  }
}

void InitScene(void) {
  uint8_t text[20];
  
  // Clear the entire screen once
  ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 0, ST7735Ctx.Width, ST7735Ctx.Height, LIGHTBLUE); // Sky
  
  
  // Initial ground drawing
  ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, groundLevel, ST7735Ctx.Width, ST7735Ctx.Height - groundLevel, GREEN);
  ST7735_LCD_Driver.DrawHLine(&st7735_pObj, 0, groundLevel, ST7735Ctx.Width, BROWN);
}

// Update fox position based on physics
void UpdatePhysics(void) {
  // Check if KEY button is pressed and fox is not already jumping
  if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET && !isJumping) {
      isJumping = 1;
      foxVelocityY = -8; // Negative velocity for upward movement
  }
  
  // Only update if fox is jumping
  if (isJumping) {
      // Update position based on velocity
      foxY += foxVelocityY;
      
      // Apply gravity
      foxVelocityY += 1;
      
      // Check if back on ground
      if (foxY >= 65) {
          foxY = 65;          // Reset to ground position
          foxVelocityY = 0;   // Stop vertical movement
          isJumping = 0;      // No longer jumping
      }
  }
}

void DrawBush(uint16_t x, uint16_t y) {
  // Bush base - positioned to be on ground level with height aligned with fox
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x, y - 5, 16, 15, BRRED);
  
  // Bush top (rounded effect with smaller rectangles)
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 2, y - 9, 12, 4, GREEN);
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 4, y - 11, 8, 2, GREEN);
  
  // Bush details
  ST7735_LCD_Driver.FillRect(&st7735_pObj, x + 7, y - 13, 2, 2, RED); // A little flower/berry
}

// Initialize obstacle system
void InitObstacles() {
  for (int i = 0; i < MAX_VISIBLE_OBSTACLES; i++) {
    obstacleActive[i] = 0;
    obstaclePosX[i] = 0;
  }
  nextObstacleIndex = 0;
  lastObstacleTime = HAL_GetTick();
}

// Update and manage obstacles
void UpdateObstacles() {
  uint32_t currentTime = HAL_GetTick();
  
  // Spawn new obstacle every obstacleInterval milliseconds
  if (currentTime - lastObstacleTime > obstacleInterval) {
    // Always create a new obstacle using circular buffer
    obstacleActive[nextObstacleIndex] = 1;
    obstaclePosX[nextObstacleIndex] = ST7735Ctx.Width; // Start from right edge
    
    // Move to next buffer position (circular)
    nextObstacleIndex = (nextObstacleIndex + 1) % MAX_VISIBLE_OBSTACLES;
    
    // Update spawn timer
    lastObstacleTime = currentTime;
  }
  
  // Update obstacle positions and check collisions
  for (int i = 0; i < MAX_VISIBLE_OBSTACLES; i++) {
    if (obstacleActive[i]) {
      // Move obstacle to the left
      obstaclePosX[i] -= scrollSpeed;
      
      // Check if obstacle is off screen
      if (obstaclePosX[i] < -20) {
        obstacleActive[i] = 0; // Deactivate
      }
      
      // Check collision with fox
      if (obstaclePosX[i] > foxX - 15 && obstaclePosX[i] < foxX + 20) {
        // Vertical collision check - if fox is not jumping high enough
        if (foxY > groundLevel - 20) {
          // Collision detected!
          // gameOver = 1; // Uncomment this to implement game over
        }
      }
    }
  }
}

// Draw all active obstacles
void DrawObstacles() {
  for (int i = 0; i < MAX_VISIBLE_OBSTACLES; i++) {
    if (obstacleActive[i]) {
      DrawBush(obstaclePosX[i], groundLevel);
    }
  }
}




/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  LED_Blink(500,500);
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
