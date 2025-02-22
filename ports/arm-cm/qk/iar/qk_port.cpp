/*============================================================================
* QK/C++ port to ARM Cortex-M, IAR
* Copyright (C) 2005 Quantum Leaps, LLC. All rights reserved.
*
* SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-QL-commercial
*
* This software is dual-licensed under the terms of the open source GNU
* General Public License version 3 (or any later version), or alternatively,
* under the terms of one of the closed source Quantum Leaps commercial
* licenses.
*
* The terms of the open source GNU General Public License version 3
* can be found at: <www.gnu.org/licenses/gpl-3.0>
*
* The terms of the closed source Quantum Leaps commercial licenses
* can be found at: <www.state-machine.com/licensing>
*
* Redistributions in source code must retain this top-level comment block.
* Plagiarizing this software to sidestep the license obligations is illegal.
*
* Contact information:
* <www.state-machine.com>
* <info@state-machine.com>
============================================================================*/
/*!
* @date Last updated on: 2022-05-13
* @version Last updated for: @ref qpcpp_7_0_1
*
* @file
* @brief QK/C++ port to ARM Cortex-M, IAR-ARM toolset
*/
/* This QK port is part of the interanl QP implementation */
#define QP_IMPL 1U
#include "qf_port.hpp"

extern "C" {

/* prototypes --------------------------------------------------------------*/
void PendSV_Handler(void);
#ifdef QK_USE_IRQ_HANDLER           /* if use IRQ... */
void QK_USE_IRQ_HANDLER(void);
#else                               /* use default (NMI) */
void NMI_Handler(void);
#endif

#define SCnSCB_ICTR  ((uint32_t volatile *)0xE000E004)
#define SCB_SYSPRI   ((uint32_t volatile *)0xE000ED14)
#define NVIC_EN      ((uint32_t volatile *)0xE000E100)
#define NVIC_IP      ((uint8_t  volatile *)0xE000E400)
#define NVIC_PEND    0xE000E200
#define NVIC_ICSR    0xE000ED04

/* helper macros to "stringify" values */
#define VAL(x) #x
#define STRINGIFY(x) VAL(x)

/*
* Initialize the exception priorities and IRQ priorities to safe values.
*
* Description:
* On Cortex-M3/M4/M7, this QK port disables interrupts by means of the
* BASEPRI register. However, this method cannot disable interrupt
* priority zero, which is the default for all interrupts out of reset.
* The following code changes the SysTick priority and all IRQ priorities
* to the safe value QF_BASEPRI, wich the QF critical section can disable.
* This avoids breaching of the QF critical sections in case the
* application programmer forgets to explicitly set priorities of all
* "kernel aware" interrupts.
*
* The interrupt priorities established in QK_init() can be later
* changed by the application-level code.
*/
void QK_init(void) {
#if (__ARM_ARCH != 6) /* if ARMv7-M or higher... */

    /* set exception priorities to QF_BASEPRI...
    * SCB_SYSPRI1: Usage-fault, Bus-fault, Memory-fault
    */
    SCB_SYSPRI[1] = (SCB_SYSPRI[1]
        | (QF_BASEPRI << 16) | (QF_BASEPRI << 8) | QF_BASEPRI);

    /* SCB_SYSPRI2: SVCall */
    SCB_SYSPRI[2] = (SCB_SYSPRI[2] | (QF_BASEPRI << 24));

    /* SCB_SYSPRI3:  SysTick, PendSV, Debug */
    SCB_SYSPRI[3] = (SCB_SYSPRI[3]
        | (QF_BASEPRI << 24) | (QF_BASEPRI << 16) | QF_BASEPRI);

    /* set all implemented IRQ priories to QF_BASEPRI... */
    uint8_t nprio = (8U + ((*SCnSCB_ICTR & 0x7U) << 3U))*4;
    for (uint8_t n = 0U; n < nprio; ++n) {
        NVIC_IP[n] = QF_BASEPRI;
    }

#endif /* ARMv7-M or higher */

    /* SCB_SYSPRI3: PendSV set to priority 0xFF (lowest) */
    SCB_SYSPRI[3] = (SCB_SYSPRI[3] | (0xFFU << 16U));

#ifdef QK_USE_IRQ_NUM
    /* The QK port is configured to use a given ARM Cortex-M IRQ #
    * to return to thread mode (default is to use the NMI exception)
    */
    NVIC_IP[QK_USE_IRQ_NUM] = 0U; /* priority 0 (highest) */
    NVIC_EN[QK_USE_IRQ_NUM / 32U] = (1U << (QK_USE_IRQ_NUM % 32U));
#endif
}

/*==========================================================================*/
/* The PendSV_Handler exception handler is used for handling context switch
* and asynchronous preemption in QK. The use of the PendSV exception is
* the recommended and most efficient method for performing context switches
* with ARM Cortex-M.
*
* The PendSV exception should have the lowest priority in the whole system
* (0xFF, see QK_init). All other exceptions and interrupts should have higher
* priority. For example, for NVIC with 2 priority bits all interrupts and
* exceptions must have numerical value of priority lower than 0xC0. In this
* case the interrupt priority levels available to your applications are (in
* the order from the lowest urgency to the highest urgency): 0x80, 0x40, 0x00.
*
* Also, *all* "kernel aware" ISRs in the QK application must call the
* QK_ISR_EXIT() macro, which triggers PendSV when it detects a need for
* a context switch or asynchronous preemption.
*
* Due to tail-chaining and its lowest priority, the PendSV exception will be
* entered immediately after the exit from the *last* nested interrupt (or
* exception). In QK, this is exactly the time when the QK activator needs to
* handle the asynchronous preemption.
*/
__stackless
void PendSV_Handler(void) {
__asm volatile (

    /* Prepare constants in registers before entering critical section */
    "  LDR     r3,=" STRINGIFY(NVIC_ICSR) "\n" /* Interrupt Control and State */
    "  MOVS    r1,#1            \n"
    "  LSLS    r1,r1,#27        \n" /* r0 := (1 << 27) (UNPENDSVSET bit) */

    /*<<<<<<<<<<<<<<<<<<<<<<< CRITICAL SECTION BEGIN <<<<<<<<<<<<<<<<<<<<<<<<*/
#if (__ARM_ARCH == 6)               /* if ARMv6-M... */
    "  CPSID   i                \n" /* disable interrupts (set PRIMASK) */
#else                               /* ARMv7-M and higher */
#if (__ARM_FP != 0)                 /* if VFP available... */
    "  PUSH    {r0,lr}          \n" /* ... push lr plus stack-aligner */
#endif                              /* VFP available */
    "  MOVS    r0,#" STRINGIFY(QF_BASEPRI) "\n"
    "  CPSID   i                \n" /* disable interrutps with BASEPRI */
    "  MSR     BASEPRI,r0       \n" /* apply the Cortex-M7 erraturm */
    "  CPSIE   i                \n" /* 837070, see SDEN-1068427. */
#endif                              /* ARMv7-M and higher */

    /* The PendSV exception handler can be preempted by an interrupt,
    * which might pend PendSV exception again. The following write to
    * ICSR[27] un-pends any such spurious instance of PendSV.
    */
    "  STR     r1,[r3]          \n" /* ICSR[27] := 1 (unpend PendSV) */

    /* The QK activator must be called in a Thread mode, while this code
    * executes in the Handler mode of the PendSV exception. The switch
    * to the Thread mode is accomplished by returning from PendSV using
    * a fabricated exception stack frame, where the return address is
    * QK_activate_().
    *
    * NOTE: the QK activator is called with interrupts DISABLED and also
    * returns with interrupts DISABLED.
    */
    "  LSRS    r3,r1,#3         \n" /* r3 := (r1 >> 3), set the T bit (new xpsr) */
    "  LDR     r2,=QK_activate_ \n" /* address of QK_activate_ */
    "  SUBS    r2,r2,#1         \n" /* align Thumb-address at halfword (new pc) */
    "  LDR     r1,=QK_thread_ret \n" /* return address after the call  (new lr) */

    "  SUB     sp,sp,#8*4       \n" /* reserve space for exception stack frame */
    "  ADD     r0,sp,#5*4       \n" /* r0 := 5 registers below the SP */
    "  STM     r0!,{r1-r3}      \n" /* save xpsr,pc,lr */

    "  MOVS    r0,#6            \n"
    "  MVNS    r0,r0            \n" /* r0 := ~6 == 0xFFFFFFF9 */
#if (__ARM_ARCH != 6)               /* if ARMv7-M and higher... */
    "  DSB                      \n" /* ARM Erratum 838869 */
#endif                              /* ARMv7-M and higher */
    "  BX      r0               \n" /* exception-return to the QK activator */
    );
}

/*==========================================================================*/
/* QK_thread_ret is a helper function executed when the QXK activator returns.
*
* NOTE: QK_thread_ret does not execute in the PendSV context!
* NOTE: QK_thread_ret is entered with interrupts DISABLED.
*/

__stackless
void QK_thread_ret(void) {
__asm volatile (

    /* After the QK activator returns, we need to resume the preempted
    * thread. However, this must be accomplished by a return-from-exception,
    * while we are still in the thread context. The switch to the exception
    * contex is accomplished by triggering the SVC or NMI exception.
    */

#if (__ARM_ARCH == 6)               /* if ARMv6-M... */
    "  CPSIE   i                \n" /* enable interrupts (clear PRIMASK) */
#else                               /* ARMv7-M or higher */
    "  MOVS    r0,#0            \n"
    "  MSR     BASEPRI,r0       \n" /* enable interrupts (clear BASEPRI) */
#if (__ARM_FP != 0)                 /* if VFP available... */
    /* make sure that the VFP stack frame will NOT be used */
    "  MRS     r0,CONTROL       \n" /* r0 := CONTROL */
    "  BICS    r0,r0,#4         \n" /* r0 := r0 & ~4 (FPCA bit) */
    "  MSR     CONTROL,r0       \n" /* CONTROL := r0 (clear CONTROL[2] FPCA bit) */
    "  ISB                      \n" /* ISB after MSR CONTROL (ARM AN321,Sect.4.16) */
#endif                              /* VFP available */
#endif                              /* ARMv7-M or higher */

#ifdef QK_USE_IRQ_NUM               /* if use IRQ... */
    "  LDR     r0,=" STRINGIFY(NVIC_PEND + (QK_USE_IRQ_NUM / 32)) "\n"
    "  MOVS    r1,#1            \n"
    /* NOTE: the following IRQ bit calculation should be done simply as
    * (QK_USE_IRQ_NUM % 32), but the IAR assembler does not accept it.
    * As a workaround the modulo (%) operation is replaced with the following:
    */
    "  LSLS    r1,r1,#" STRINGIFY(QK_USE_IRQ_NUM - (QK_USE_IRQ_NUM/32)*32) "\n"
    "  STR     r1,[r0]          \n" /* pend the IRQ */
    "  B       .                \n" /* wait for preemption by the IRQ */
#else                               /* use the NMI */
    "  LDR     r0,=" STRINGIFY(NVIC_ICSR) "\n" /* Interrupt Control and State */
    "  MOVS    r1,#1            \n"
    "  LSLS    r1,r1,#31        \n" /* r1 := (1 << 31) (NMI bit) */
    "  STR     r1,[r0]          \n" /* ICSR[31] := 1 (pend NMI) */
    "  B       .                \n" /* wait for preemption by NMI */
#endif                              /* use NMI */
    );
}

/*==========================================================================*/
/* This exception handler is used for returning back to the interrupted task.
* The exception handler simply removes its own interrupt stack frame from
* the stack (MSP) and returns to the preempted task using the interrupt
* stack frame that must be at the top of the stack.
*/
__stackless
#ifdef QK_USE_IRQ_HANDLER           /* if use IRQ... */
void QK_USE_IRQ_HANDLER(void)
#else                               /* use NMI */
void NMI_Handler(void)
#endif                              /* use NMI */
{
__asm volatile (
    "  ADD     sp,sp,#(8*4)     \n" /* remove one 8-register exception frame */

#if (__ARM_ARCH != 6)               /* if ARMv7-M and higher... */
#if (__ARM_FP != 0)                 /* if VFP available... */
    "  POP     {r0,lr}          \n" /* pop stack aligner and EXC_RETURN to LR */
    "  DSB                      \n" /* ARM Erratum 838869 */
#endif                              /* VFP available */
#endif                              /* ARMv7-M and higher */
    "  BX      lr               \n" /* return to the preempted task */
    );
}

/*==========================================================================*/
#if (__ARM_ARCH == 6) /* if ARMv6-M... */

/* hand-optimized quick LOG2 in assembly (No CLZ instruction in ARMv6-M) */
uint_fast8_t QF_qlog2(uint32_t x) {
    static uint8_t const log2LUT[16] = {
        0U, 1U, 2U, 2U, 3U, 3U, 3U, 3U,
        4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U
    };
    uint_fast8_t n;
    __asm (
        "MOVS    %[n],#0\n"
#if (QF_MAX_ACTIVE > 16U)
        "LSRS    r2,r0,#16\n"
        "BEQ.N   QF_qlog2_1\n"
        "MOVS    %[n],#16\n"
        "MOVS    r0,r2\n"
    "QF_qlog2_1:\n"
#endif
#if (QF_MAX_ACTIVE > 8U)
        "LSRS    r2,r0,#8\n"
        "BEQ.N   QF_qlog2_2\n"
        "ADDS    %[n],%[n],#8\n"
        "MOVS    r0,r2\n"
    "QF_qlog2_2:\n"
#endif
        "LSRS    r2,r0,#4\n"
        "BEQ.N   QF_qlog2_3\n"
        "ADDS    %[n],%[n],#4\n"
        "MOVS    r0,r2\n"
    "QF_qlog2_3:"
        : [n]"=r"(n)
    );
    return n + log2LUT[x];
}

#endif /* ARMv6-M */

} // extern "C"

