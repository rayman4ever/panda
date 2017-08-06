#ifdef STM32F4
  #include "stm32f4xx_hal_gpio_ex.h"
#else
  #include "stm32f2xx_hal_gpio_ex.h"
#endif

#include "llgpio.h"

void set_can_enable(CAN_TypeDef *CAN, int enabled) {
  // enable CAN busses
  if (CAN == CAN1) {
    #ifdef PANDA
      // CAN1_EN
      set_gpio_output(GPIOC, 1, !enabled);
    #else
      // CAN1_EN
      set_gpio_output(GPIOB, 3, enabled);
    #endif
  } else if (CAN == CAN2) {
    #ifdef PANDA
      // CAN2_EN
      set_gpio_output(GPIOC, 13, !enabled);
    #else
      // CAN2_EN
      set_gpio_output(GPIOB, 4, enabled);
    #endif
  #ifdef CAN3
  } else if (CAN == CAN3) {
    // CAN3_EN
    set_gpio_output(GPIOA, 0, !enabled);
  #endif
  }
}

#ifdef PANDA
  #define LED_RED 9
  #define LED_GREEN 7
  #define LED_BLUE 6
#else
  #define LED_RED 10
  #define LED_GREEN 11
  #define LED_BLUE -1
#endif

void set_led(int led_num, int on) {
  if (led_num == -1) return;

  #ifdef PANDA
    set_gpio_output(GPIOC, led_num, !on);
  #else
    set_gpio_output(GPIOB, led_num, !on);
  #endif
}


// TODO: does this belong here?
void periph_init() { 
  // enable GPIOB, UART2, CAN, USB clock
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
  #ifdef PANDA
    RCC->APB1ENR |= RCC_APB1ENR_UART5EN;
  #endif
  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
  #ifdef CAN3
    RCC->APB1ENR |= RCC_APB1ENR_CAN3EN;
  #endif
  RCC->APB1ENR |= RCC_APB1ENR_DACEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  //RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

  // needed?
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
}

void set_can2_mode(int use_gmlan) {
  // http://www.bittiming.can-wiki.info/#bxCAN
  // 24 MHz, sample point at 87.5%
  uint32_t pclk = 24000;
  uint32_t num_time_quanta = 16;
  uint32_t prescaler;

  // connects to CAN2 xcvr or GMLAN xcvr
  if (use_gmlan) {
    // B5,B6: disable normal mode
    set_gpio_mode(GPIOB, 5, MODE_INPUT);
    set_gpio_mode(GPIOB, 6, MODE_INPUT);

    // B12,B13: gmlan mode
    set_gpio_alternate(GPIOB, 12, GPIO_AF9_CAN2);
    set_gpio_alternate(GPIOB, 13, GPIO_AF9_CAN2);

    /* GMLAN mode pins:
    M0(B15)  M1(B14)  mode
    =======================
    0        0        sleep
    1        0        100kbit
    0        1        high voltage wakeup
    1        1        33kbit (normal)
    */

    // put gmlan transceiver in normal mode
    set_gpio_output(GPIOB, 14, 1);
    set_gpio_output(GPIOB, 15, 1);

    // 83.3 kbps
    // prescaler = pclk / num_time_quanta * 10 / 833;

    // 33.3 kbps
    prescaler = pclk / num_time_quanta * 10 / 333;
  } else {
    // B12,B13: disable gmlan mode
    set_gpio_mode(GPIOB, 12, MODE_INPUT);
    set_gpio_mode(GPIOB, 13, MODE_INPUT);

    // B5,B6: normal mode
    set_gpio_alternate(GPIOB, 5, GPIO_AF9_CAN2);
    set_gpio_alternate(GPIOB, 6, GPIO_AF9_CAN2);

    // 500 kbps
    prescaler = pclk / num_time_quanta / 500;
  }

  // init
  CAN2->MCR = CAN_MCR_TTCM | CAN_MCR_INRQ;
  while((CAN2->MSR & CAN_MSR_INAK) != CAN_MSR_INAK);

  // set speed
  // seg 1: 13 time quanta, seg 2: 2 time quanta
  CAN2->BTR = (CAN_BTR_TS1_0 * 12) |
    CAN_BTR_TS2_0 | (prescaler - 1);

  // running
  CAN2->MCR = CAN_MCR_TTCM;
  while((CAN2->MSR & CAN_MSR_INAK) == CAN_MSR_INAK);
}

// Prerequisites to use single-wire GMLAN:
// - PB8 & PB9 (CAN1) are wired to a RX & TX of single-wire
//   GMLAN transceiver NCV7356 circuitry:
//   https://www.onsemi.com/pub/Collateral/NCV7356-D.PDF
// - PB3 and PB7 are wired to Mode0 and mode1 pins of the
//   transceiver respectively.
void set_can1_mode(int use_gmlan) {

  // http://www.bittiming.can-wiki.info/#bxCAN
  // 24 MHz, sample point at 87.5%
  uint32_t pclk = 24000;
  uint32_t num_time_quanta = 16;
  uint32_t prescaler;

  if (use_gmlan) {
    // 33.3 kbps
    prescaler = pclk / num_time_quanta * 10 / 333;

    // pull up RX
    GPIOB->PUPDR |= GPIO_PUPDR_PUPDR8_0;

    // Both mode pins to high means 33.3kbps mode
    set_gpio_mode(GPIOB, 3, MODE_OUTPUT);
    set_gpio_mode(GPIOB, 7, MODE_OUTPUT);
    set_gpio_output(GPIOB, 3, 1);
    set_gpio_output(GPIOB, 7, 1);
  } else {
    // 500 kbps
    prescaler = pclk / num_time_quanta / 500;

    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR8_0;
  }

  CAN_TypeDef *CAN = CAN1;

  // init
  CAN->MCR = CAN_MCR_TTCM | CAN_MCR_INRQ;
  while((CAN->MSR & CAN_MSR_INAK) != CAN_MSR_INAK);

  // set speed
  // seg 1: 13 time quanta, seg 2: 2 time quanta
  CAN->BTR = (CAN_BTR_TS1_0 * 12) |
    CAN_BTR_TS2_0 | (prescaler - 1);

  // running
  CAN->MCR = CAN_MCR_TTCM;

  // unlike Panda's approach of switching transceivers,
  // current setup is for CAN1 to be hardwired to a single
  // transceiver, therefore setting a wrong mode will never succeed
  int tick = 0;
  #define CAN_TIMEOUT 1000000
  while((CAN->MSR & CAN_MSR_INAK) == CAN_MSR_INAK &&
    tick++ < CAN_TIMEOUT);
}

// board specific
void gpio_init() {
  // pull low to hold ESP in reset??
  // enable OTG out tied to ground
  GPIOA->ODR = 0;
  GPIOB->ODR = 0;
  GPIOA->PUPDR = 0;
  //GPIOC->ODR = 0;
  GPIOB->AFR[0] = 0;
  GPIOB->AFR[1] = 0;

  // C2,C3: analog mode, voltage and current sense
  set_gpio_mode(GPIOC, 2, MODE_ANALOG);
  set_gpio_mode(GPIOC, 3, MODE_ANALOG);

  // C8: FAN aka TIM3_CH4
  set_gpio_alternate(GPIOC, 8, GPIO_AF2_TIM3);

  // turn off LEDs and set mode
  set_led(LED_RED, 0);
  set_led(LED_GREEN, 0);
  set_led(LED_BLUE, 0);

  // A11,A12: USB
  set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG_FS);
  set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG_FS);
  GPIOA->OSPEEDR = GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12;

#ifdef PANDA
  // enable started_alt on the panda
  set_gpio_pullup(GPIOA, 1, PULL_UP);

  // A2,A3: USART 2 for debugging
  set_gpio_alternate(GPIOA, 2, GPIO_AF7_USART2);
  set_gpio_alternate(GPIOA, 3, GPIO_AF7_USART2);

  // A9,A10: USART 1 for talking to the ESP
  set_gpio_alternate(GPIOA, 9, GPIO_AF7_USART1);
  set_gpio_alternate(GPIOA, 10, GPIO_AF7_USART1);

  // B12: GMLAN, ignition sense, pull up
  set_gpio_pullup(GPIOB, 12, PULL_UP);

  // A4,A5,A6,A7: setup SPI
  set_gpio_alternate(GPIOA, 4, GPIO_AF5_SPI1);
  set_gpio_alternate(GPIOA, 5, GPIO_AF5_SPI1);
  set_gpio_alternate(GPIOA, 6, GPIO_AF5_SPI1);
  set_gpio_alternate(GPIOA, 7, GPIO_AF5_SPI1);
#endif

  // B8,B9: CAN 1
  set_can_enable(CAN1, 0);
#ifdef STM32F4
  set_gpio_alternate(GPIOB, 8, GPIO_AF8_CAN1);
  set_gpio_alternate(GPIOB, 9, GPIO_AF8_CAN1);
#else
  set_gpio_alternate(GPIOB, 8, GPIO_AF9_CAN1);
  set_gpio_alternate(GPIOB, 9, GPIO_AF9_CAN1);
#endif

  // B5,B6: CAN 2
  set_can_enable(CAN2, 0);
  set_can2_mode(0);

  // A8,A15: CAN3
  #ifdef CAN3
    set_can_enable(CAN3, 0);
    set_gpio_alternate(GPIOA, 8, GPIO_AF11_CAN3);
    set_gpio_alternate(GPIOA, 15, GPIO_AF11_CAN3);
  #endif

  #ifdef PANDA
    // K-line enable moved from B4->B7 to make room for GMLAN on CAN3
    #ifdef REVC
      set_gpio_output(GPIOB, 7, 1);
    #else
      set_gpio_output(GPIOB, 4, 1);
    #endif

    // C12,D2: K-Line setup on UART 5
    set_gpio_alternate(GPIOC, 12, GPIO_AF8_UART5);
    set_gpio_alternate(GPIOD, 2, GPIO_AF8_UART5);
    set_gpio_pullup(GPIOD, 2, PULL_UP);

    // L-line enable
    set_gpio_output(GPIOA, 14, 1);

    // C10,C11: L-Line setup on USART 3
    set_gpio_alternate(GPIOC, 10, GPIO_AF7_USART3);
    set_gpio_alternate(GPIOC, 11, GPIO_AF7_USART3);
    set_gpio_pullup(GPIOC, 11, PULL_UP);
  #endif

  #ifdef REVC
    // B2,A13: set DCP mode on the charger (breaks USB!)
    set_gpio_output(GPIOB, 2, 0);
    set_gpio_output(GPIOA, 13, 0);
  #endif
}

