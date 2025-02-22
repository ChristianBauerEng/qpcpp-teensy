// QP/C++ Real-Time Embedded Framework (RTEF)
// Copyright (C) 2005 Quantum Leaps, LLC. All rights reserved.
//
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-QL-commercial
//
// This software is dual-licensed under the terms of the open source GNU
// General Public License version 3 (or any later version), or alternatively,
// under the terms of one of the closed source Quantum Leaps commercial
// licenses.
//
// The terms of the open source GNU General Public License version 3
// can be found at: <www.gnu.org/licenses/gpl-3.0>
//
// The terms of the closed source Quantum Leaps commercial licenses
// can be found at: <www.state-machine.com/licensing>
//
// Redistributions in source code must retain this top-level comment block.
// Plagiarizing this software to sidestep the license obligations is illegal.
//
// Contact information:
// <www.state-machine.com>
// <info@state-machine.com>
//============================================================================
//! @date Last updated on: 2022-05-02
//! @version Last updated for: @ref qpcpp_7_0_0
//!
//! @file
//! @brief QXK/C++ port to ARM Cortex-M, QXK kernel, IAR-ARM toolset
//!
#ifndef QXK_PORT_HPP
#define QXK_PORT_HPP

// determination if the code executes in the ISR context
#define QXK_ISR_CONTEXT_() (__get_IPSR() != 0U)

// trigger the PendSV exception to pefrom the context switch
#define QXK_CONTEXT_SWITCH_()  \
    *Q_UINT2PTR_CAST(uint32_t, 0xE000ED04U) = (1U << 28U)

// QXK ISR entry and exit
#define QXK_ISR_ENTRY() ((void)0)

#define QXK_ISR_EXIT()  do {   \
    QF_INT_DISABLE();          \
    if (QXK_sched_() != 0U) {  \
        QXK_CONTEXT_SWITCH_(); \
    }                          \
    QF_INT_ENABLE();           \
    QXK_ARM_ERRATUM_838869();  \
} while (false)

#if (__ARM_ARCH == 6) // ARMv6-M?
    #define QXK_ARM_ERRATUM_838869() ((void)0)
#else // Cortex-M3/M4/M7 (v7-M)
    // The following macro implements the recommended workaround for the
    // ARM Erratum 838869. Specifically, for Cortex-M3/M4/M7 the DSB
    // (memory barrier) instruction needs to be added before exiting an ISR.
    //
    #define QXK_ARM_ERRATUM_838869() __DSB()
#endif // ARMv6-M

// Use a given ARM Cortex-M IRQ to return to thread mode (default NMI)
//
// NOTE:
// If you need the NMI for other purposes, you can define the macros
// QXK_USE_IRQ_NUM and QXK_USE_IRQ_HANDLER to use thus specified IRQ
// instead of the NMI (the IRQ should not be used for anything else).
// These two macros can be defined on the command line to the compiler
// and are actually needed only to compile the qxk_port.c file.
//
//#define QXK_USE_IRQ_NUM     25
//#define QXK_USE_IRQ_HANDLER CRYPTO_IRQHandler

// initialization of the QXK kernel
#define QXK_INIT() QXK_init()
extern "C" void QXK_init(void);
extern "C" void QXK_thread_ret(void);

#include "qxk.hpp" // QXK platform-independent public interface

#endif // QXK_PORT_HPP

