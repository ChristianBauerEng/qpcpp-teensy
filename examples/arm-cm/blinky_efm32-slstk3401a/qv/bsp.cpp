//============================================================================
// Product: "Blinky" on EFM32-SLSTK3401A board, cooperative QV kernel
// Last updated for version 6.6.0
// Last updated on  2019-10-14
//
//                    Q u a n t u m  L e a P s
//                    ------------------------
//                    Modern Embedded Software
//
// Copyright (C) 2005-2019 Quantum Leaps. All rights reserved.
//
// This program is open source software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Alternatively, this program may be distributed and modified under the
// terms of Quantum Leaps commercial licenses, which expressly supersede
// the GNU General Public License and are specifically designed for
// licensees interested in retaining the proprietary status of their code.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <www.gnu.org/licenses>.
//
// Contact information:
// <www.state-machine.com>
// <info@state-machine.com>
//============================================================================
#include "qpcpp.hpp"
#include "blinky.hpp"
#include "bsp.hpp"

#include "em_device.h"  // the device specific header (SiLabs)
#include "em_cmu.h"     // Clock Management Unit (SiLabs)
#include "em_gpio.h"    // GPIO (SiLabs)
// add other drivers if necessary...

#ifdef Q_SPY
    #error Simple Blinky Application does not provide Spy build configuration
#endif

//Q_DEFINE_THIS_FILE

// Local-scope objects -------------------------------------------------------
#define LED0_PIN    4
#define LED0_PORT   gpioPortF

#define LED1_PIN    5
#define LED1_PORT   gpioPortF

#define BTN_SW1     (1U << 4)
#define BTN_SW2     (1U << 0)

// ISRs used in this project =================================================
extern "C" {

//............................................................................
void SysTick_Handler(void); // prototype
void SysTick_Handler(void) {
    QF::TICK_X(0U, nullptr); // process time events for rate 0
}

} // extern "C"

// BSP functions =============================================================
void BSP_init(void) {
    // NOTE: SystemInit() already called from the startup code
    //  but SystemCoreClock needs to be updated
    //
    SystemCoreClockUpdate();

    // configure the FPU usage by choosing one of the options...
    //
    // Do NOT to use the automatic FPU state preservation and
    // do NOT to use the FPU lazy stacking.
    //
    // NOTE:
    // Use the following setting when FPU is used only by active objects
    // and NOT in any ISR. This setting is very efficient, but if any ISRs
    // start using the FPU, this can lead to corruption of the FPU registers.
    //
    FPU->FPCCR &= ~((1U << FPU_FPCCR_ASPEN_Pos) | (1U << FPU_FPCCR_LSPEN_Pos));

    // enable clock for to the peripherals used by this application...
    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);

    // configure the LEDs
    GPIO_PinModeSet(LED0_PORT, LED0_PIN, gpioModePushPull, 0);
    GPIO_PinModeSet(LED1_PORT, LED1_PIN, gpioModePushPull, 0);
    GPIO_PinOutClear(LED0_PORT, LED0_PIN);
    GPIO_PinOutClear(LED1_PORT, LED1_PIN);

    // configure the Buttons
    //...
}
//............................................................................
void BSP_ledOff(void) {
    //GPIO_PinOutClear(LED0_PORT, LED0_PIN);
    GPIO->P[LED0_PORT].DOUT &= ~(1U << LED0_PIN);
}
//............................................................................
void BSP_ledOn(void) {
    // exercise the FPU with some floating point computations
    float volatile x = 3.1415926F;
    x = x + 2.7182818F;

    //GPIO_PinOutSet(LED0_PORT, LED0_PIN);
    GPIO->P[LED0_PORT].DOUT |= (1U << LED0_PIN);
}


// QF callbacks ==============================================================
void QF::onStartup(void) {
    // set up the SysTick timer to fire at BSP_TICKS_PER_SEC rate
    SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);

    // assing all priority bits for preemption-prio. and none to sub-prio.
    NVIC_SetPriorityGrouping(0U);

    // set priorities of ALL ISRs used in the system, see NOTE00
    NVIC_SetPriority(SysTick_IRQn,  QF_AWARE_ISR_CMSIS_PRI);
    // ...

    // enable IRQs...
}
//............................................................................
void QF::onCleanup(void) {
}
//............................................................................
void QV::onIdle(void) { // CATION: called with interrupts DISABLED, NOTE01
    // toggle LED1 on and then off, see NOTE02
    GPIO_PinOutSet(LED1_PORT, LED1_PIN);
    //GPIO->P[LED1_PORT].DOUT |= (1U << LED1_PIN);
    GPIO_PinOutClear(LED1_PORT, LED1_PIN);
    //GPIO->P[LED1_PORT].DOUT &= ~(1U << LED1_PIN);

#ifdef NDEBUG
    // Put the CPU and peripherals to the low-power mode.
    // you might need to customize the clock management for your application,
    // see the datasheet for your particular Cortex-M MCU.
    //
    QV_CPU_SLEEP();  // atomically go to sleep and enable interrupts
#else
    QF_INT_ENABLE(); // just enable interrupts
#endif
}

//............................................................................
extern "C" Q_NORETURN Q_onAssert(char const * const module, int_t const loc) {
    //
    // NOTE: add here your application-specific error handling
    //
    (void)module;
    (void)loc;
    QS_ASSERTION(module, loc, 10000U);
    NVIC_SystemReset();
}

//============================================================================
// NOTE00:
// The QF_AWARE_ISR_CMSIS_PRI constant from the QF port specifies the highest
// ISR priority that is disabled by the QF framework. The value is suitable
// for the NVIC_SetPriority() CMSIS function.
//
// Only ISRs prioritized at or below the QF_AWARE_ISR_CMSIS_PRI level (i.e.,
// with the numerical values of priorities equal or higher than
// QF_AWARE_ISR_CMSIS_PRI) are allowed to call any QF services. These ISRs
// are "QF-aware".
//
// Conversely, any ISRs prioritized above the QF_AWARE_ISR_CMSIS_PRI priority
// level (i.e., with the numerical values of priorities less than
// QF_AWARE_ISR_CMSIS_PRI) are never disabled and are not aware of the kernel.
// Such "QF-unaware" ISRs cannot call any QF services. The only mechanism
// by which a "QF-unaware" ISR can communicate with the QF framework is by
// triggering a "QF-aware" ISR, which can post/publish events.
//
// NOTE01:
// The QV::onIdle() callback is called with interrupts disabled, because the
// determination of the idle condition might change by any interrupt posting
// an event. QV::onIdle() must internally enable interrupts, ideally
// atomically with putting the CPU to the power-saving mode.
//
// NOTE02:
// One of the LEDs is used to visualize the idle loop activity. The brightness
// of the LED is proportional to the frequency of invcations of the idle loop.
// Please note that the LED is toggled with interrupts locked, so no interrupt
// execution time contributes to the brightness of the User LED.
//
