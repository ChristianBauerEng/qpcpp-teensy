//! @file
//! @brief QK/C++ port to ARM Cortex-R, preemptive QK kernel, IAR-ARM toolset
//! @cond
//============================================================================
//! Last updated for version 6.6.0
//! Last updated on  2019-07-30
//!
//!                    Q u a n t u m  L e a P s
//!                    ------------------------
//!                    Modern Embedded Software
//!
//! Copyright (C) 2005-2019 Quantum Leaps. All rights reserved.
//!
//! This program is open source software: you can redistribute it and/or
//! modify it under the terms of the GNU General Public License as published
//! by the Free Software Foundation, either version 3 of the License, or
//! (at your option) any later version.
//!
//! Alternatively, this program may be distributed and modified under the
//! terms of Quantum Leaps commercial licenses, which expressly supersede
//! the GNU General Public License and are specifically designed for
//! licensees interested in retaining the proprietary status of their code.
//!
//! This program is distributed in the hope that it will be useful,
//! but WITHOUT ANY WARRANTY; without even the implied warranty of
//! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//! GNU General Public License for more details.
//!
//! You should have received a copy of the GNU General Public License
//! along with this program. If not, see <www.gnu.org/licenses>.
//!
//! Contact information:
//! <www.state-machine.com/licensing>
//! <info@state-machine.com>
//============================================================================
//! @endcond

#ifndef QK_PORT_HPP
#define QK_PORT_HPP

// QK-specific Interrupt Request handler BEGIN
#ifdef __ARMVFP__
#define QK_IRQ_BEGIN(name_) \
    __stackless __arm void name_(void) { \
    __asm(" SUB LR, LR, #4\n" \
    " SRSDB #31!\n" \
    " CPS #31\n" \
    " PUSH {R0-R3, R12}"); \
    __asm(" FMRX R12, FPSCR\n" \
    " STMFD SP!, {R12}\n" \
    " FMRX R12, FPEXC\n" \
    " STMFD SP!, {R12}\n" \
    " FSTMDBD SP!, {D0-D7}"); \
    __asm(" AND R3, SP, #4\n" \
    " SUB SP, SP, R3\n" \
    " PUSH {R3, LR}\n"); \
    ++QK_attr_.intNest; {
#else
#define QK_IRQ_BEGIN(name_) \
    __stackless __arm void name_(void) { \
    __asm(" SUB LR, LR, #4\n" \
    " SRSDB #31!\n" \
    " CPS #31\n" \
    " PUSH {R0-R3, R12}"); \
    __asm(" AND R3, SP, #4\n" \
    " SUB SP, SP, R3\n" \
    " PUSH {R3, LR}\n"); \
    ++QK_attr_.intNest; {
#endif

// QK-specific Interrupt Request handler END
#ifdef __ARMVFP__
#define QK_IRQ_END() \
    } --QK_attr_.intNest; \
    if (QK_attr_.intNest == (uint_fast8_t)0) { \
        if (QK_sched_() != (uint_fast8_t)0) { \
            QK_activate_(); \
        } \
    } \
    __asm(" POP {R3, LR}\n" \
    " ADD SP, SP, R3"); \
    __asm(" FLDMIAD SP!, {D0-D7}\n" \
    " LDMFD SP!, {R12}\n" \
    " FMXR FPEXC, R12 \n" \
    " LDMFD SP!, {R12} \n" \
    " FMXR FPSCR, R12"); \
    __asm(" POP {R0-R3, R12}\n" \
    " RFEIA SP!"); \
}
#else
#define QK_IRQ_END() \
    } --QK_attr_.intNest; \
    if (QK_attr_.intNest == (uint_fast8_t)0) { \
        if (QK_sched_() != (uint_fast8_t)0) { \
            QK_activate_(); \
        } \
    } \
    __asm(" POP {R3, LR}\n" \
    " ADD SP, SP, R3"); \
    __asm(" POP {R0-R3, R12}\n" \
    " RFEIA SP!"); \
}
#endif

#include "qk.hpp" // QK platform-independent public interface

#endif // QK_PORT_HPP
