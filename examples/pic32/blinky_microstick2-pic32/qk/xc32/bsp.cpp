//============================================================================
// BSP for Blinky example, Microstick II board, preemptive QK kernel, XC32
// Last updated for version 6.5.1
// Last updated on  2019-06-10
//
//                    Q u a n t u m  L e a P s
//                    ------------------------
//                    Modern Embedded Software
//
// Copyright (C) 2005-2019 Quantum Leaps, LLC. All rights reserved.
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
// along with this program. If not, see <www.gnu.org/licenses/>.
//
// Contact information:
// <www.state-machine.com/licensing>
// <info@state-machine.com>
//============================================================================
#include "qpcpp.hpp"
#include "bsp.hpp"
#include "blinky.hpp"

Q_DEFINE_THIS_FILE

#ifdef Q_SPY
    #error Simple Blinky Application does not provide Spy build configuration
#endif

#pragma config  FNOSC    = FRCPLL    // 8 MHz
#pragma config  FPLLIDIV = DIV_2     // 4 MHz
#pragma config  FPLLMUL  = MUL_20    // 80 MHz
#pragma config  FPLLODIV = DIV_2     // 40 MHz == FRC
#pragma config  FWDTEN   = OFF       // watchdog off
#pragma config  FPBDIV   = DIV_1     // same peripheral clock

// #pragma config statements should precede project file includes
#include <xc.h>   // header for PIC32 device in use
#include <sys/attribs.h>

// system clock using FRC and PLL: 40 MHz
#define SYS_FREQ         40000000U
// peripheral clock frequency
#define PER_HZ           (SYS_FREQ / 1U)

// controlling the LED of Microstick II
#define LED_ON()         (LATASET = (1U << 0))
#define LED_OFF()        (LATACLR = (1U << 0))
#define LED_TOGGLE()     (LATAINV = (1U << 0))

// namespace DPP *************************************************************
namespace DPP {

// Local-scope objects -------------------------------------------------------

// ISRs used in this project =================================================
extern "C" {

void __ISR(_TIMER_2_VECTOR, IPL4SOFT) tickISR(void) {
    QK_ISR_ENTRY(); // inform QK about the ISR entry

    IFS0CLR = _IFS0_T2IF_MASK; // clear the interrupt source

    QP::QF::TICK_X(0U, nullptr); // handle armed time events at tick rate 0

    QK_ISR_EXIT();  // inform QK about the ISR exit
}
//............................................................................
// for testing interrupt nesting and active object preemption
void __ISR(_EXTERNAL_0_VECTOR, IPL6SOFT) testISR(void) {
    static QP::QEvt const tout_evt = { TIMEOUT_SIG, 0U, 0U };

    QK_ISR_ENTRY(); // inform QK about the ISR entry

    IFS0CLR = _IFS0_INT0IF_MASK; // clear the interrupt source

    AO_Blinky->POST(&tout_evt, nullptr);

    QK_ISR_EXIT();  // inform QK about the ISR exit
}

} // extern "C"

} // namespace DPP

// BSP functions =============================================================
void BSP_init(void) {
    TRISA = 0x00; // set LED pins as outputs
    PORTA = 0x00; // set LED drive state low
}
//............................................................................
void BSP_terminate(int16_t result) {
    (void)result;
}
//............................................................................
void BSP_ledOff(void) {
    LED_OFF();
}
//............................................................................
void BSP_ledOn(void) {
    LED_ON();
}
//............................................................................
// NOTE: this implementation of the assertion handler is intended only for
// debugging and MUST be changed for deployment of the application (assuming
// that you ship your production code with assertions enabled).
//
extern "C"
Q_NORETURN Q_onAssert(char const * const file, int_t const loc) {
    (void)file;       // unused parameter
    (void)loc;        // unused parameter
    QF_INT_DISABLE(); // make sure that interrupts are disabled
    for (;;) {
    }
}

// namespace QP **************************************************************
namespace QP {

// QF callbacks ==============================================================
void QF::onStartup(void) { // entered with interrupts disabled
    INTCONSET = _INTCON_MVEC_MASK; // configure multi-vectored interrupts

    T2CON = 0x0060; // stop timer, set up for 1:64 prescaler
    TMR2 = 0;       // count from zero up to the period
    PR2 = SYS_FREQ / (BSP_TICKS_PER_SEC * 64); // set the Timer2 period
    IFS0CLR = _IFS0_T2IF_MASK;   // clear Timer2 Interrupt Flag
    IEC0SET = _IEC0_T2IE_MASK;   // enable Timer2 interrupt
    T2CONSET = _T2CON_ON_MASK;   // Start Timer2

    INTCONbits.INT0EP = 1;       // INT0 interrupt on positive edge
    IEC0SET = _IEC0_INT0IE_MASK; // enable INT0 interrupt
    IFS0CLR = _IFS0_INT0IF_MASK; // clear the interrupt for INT0

    // explicitly assign priorities to all interrupts...
    // NOTE: must match the IPLxSOFT settings in the __ISR() macros
    IPC2bits.T2IP   = 4; // Timer2 interrupt priority, must match tickISR
    IPC0bits.INT0IP = 6; // set INT0 priority; must match IPL in testISR
}
//............................................................................
void QF::onCleanup(void) {
}
//............................................................................
void QK::onIdle(void) {

    // NOTE: not enough LEDs on the Microstick II board to implement
    //  the idle loop activity indicator ...
    //LED_ON (); /* blink the IDLE LED
    //LED_OFF();

#ifdef NDEBUG
    _wait();   // execute the WAIT instruction to stop the CPU
#endif
}

} // namespace QP

