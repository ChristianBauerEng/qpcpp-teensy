//! @file
//! @brief QS/C++ port to Win32 with GNU or Visual C++ compilers
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
//!
#ifndef QS_PORT_HPP
#define QS_PORT_HPP

#define QS_TIME_SIZE        4U

#ifdef _WIN64 // 64-bit architecture?
    #define QS_OBJ_PTR_SIZE 8U
    #define QS_FUN_PTR_SIZE 8U
#else         // 32-bit architecture
    #define QS_OBJ_PTR_SIZE 4U
    #define QS_FUN_PTR_SIZE 4U
#endif

namespace QP {
void QS_output(void);    // handle the QS output
void QS_rx_input(void);  // handle the QS-RX input
} // namespace QP

//============================================================================
// NOTE: QS might be used with or without other QP components, in which
// case the separate definitions of the macros QF_CRIT_STAT_TYPE,
// QF_CRIT_ENTRY, and QF_CRIT_EXIT are needed. In this port QS is configured
// to be used with the other QP component, by simply including "qf_port.hpp"
// *before* "qs.hpp".
//
#include "qf_port.hpp" // use QS with QF
#include "qs.hpp"      // QS platform-independent public interface

#endif // QS_PORT_HPP

