//============================================================================
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
//! @date Last updated on: 2022-06-15
//! @version Last updated for: @ref qpcpp_7_0_1
//!
//! @file
//! @brief QP::QEQueue implementation

#define QP_IMPL             // this is QP implementation
#include "qf_port.hpp"      // QF port
#include "qf_pkg.hpp"       // QF package-scope interface
#include "qassert.h"        // QP embedded systems-friendly assertions
#ifdef Q_SPY                // QS software tracing enabled?
    #include "qs_port.hpp"  // QS port
    #include "qs_pkg.hpp"   // QS facilities for pre-defined trace records
#else
    #include "qs_dummy.hpp" // disable the QS software tracing
#endif // Q_SPY

// unnamed namespace for local definitions with internal linkage
namespace {

Q_DEFINE_THIS_MODULE("qf_qeq")

} // unnamed namespace

//============================================================================
namespace QP {

QEQueue::QEQueue(void) noexcept
  : m_frontEvt(nullptr),
    m_ring(nullptr),
    m_end(0U),
    m_head(0U),
    m_tail(0U),
    m_nFree(0U),
    m_nMin(0U)
{}

//............................................................................
void QEQueue::init(QEvt const *qSto[],
                   std::uint_fast16_t const qLen) noexcept
{
    m_frontEvt = nullptr; // no events in the queue
    m_ring     = &qSto[0];
    m_end      = static_cast<QEQueueCtr>(qLen);
    if (qLen > 0U) {
        m_head = 0U;
        m_tail = 0U;
    }
    m_nFree    = static_cast<QEQueueCtr>(qLen + 1U); //+1 for frontEvt
    m_nMin     = m_nFree;
}

//............................................................................
bool QEQueue::post(QEvt const * const e,
                   std::uint_fast16_t const margin,
                   std::uint_fast8_t const qs_id) noexcept
{
    //! @pre event must be valid
    Q_REQUIRE_ID(200, e != nullptr);

    QF_CRIT_STAT_
    QF_CRIT_E_();
    QEQueueCtr nFree = m_nFree; // temporary to avoid UB for volatile access

    // margin available?
    bool status;
    if (((margin == QF_NO_MARGIN) && (nFree > 0U))
        || (nFree > static_cast<QEQueueCtr>(margin)))
    {
        // is it a dynamic event?
        if (e->poolId_ != 0U) {
            QF_EVT_REF_CTR_INC_(e); // increment the reference counter
        }

        --nFree; // one free entry just used up
        m_nFree = nFree; // update the volatile
        if (m_nMin > nFree) {
            m_nMin = nFree; // update minimum so far
        }

        QS_BEGIN_NOCRIT_PRE_(QS_QF_EQUEUE_POST, qs_id)
            QS_TIME_PRE_();                     // timestamp
            QS_SIG_PRE_(e->sig);                // the signal of this event
            QS_OBJ_PRE_(this);                  // this queue object
            QS_2U8_PRE_(e->poolId_, e->refCtr_);// pool Id & refCtr of the evt
            QS_EQC_PRE_(nFree);                 // number of free entries
            QS_EQC_PRE_(m_nMin);                // min number of free entries
        QS_END_NOCRIT_PRE_()

        // is the queue empty?
        if (m_frontEvt == nullptr) {
            m_frontEvt = e; // deliver event directly
        }
        // queue is not empty, leave event in the ring-buffer
        else {
            // insert event into the ring buffer (FIFO)
            m_ring[m_head] = e; // insert e into buffer

            // need to wrap?
            if (m_head == 0U) {
                m_head = m_end; // wrap around
            }
            m_head = (m_head - 1U);
        }
        status = true; // event posted successfully
    }
    else {
        //! @note assert if event cannot be posted and dropping events is
        //! not acceptable
        Q_ASSERT_CRIT_(210, margin != QF_NO_MARGIN);

        QS_BEGIN_NOCRIT_PRE_(QS_QF_EQUEUE_POST_ATTEMPT, qs_id)
            QS_TIME_PRE_();        // timestamp
            QS_SIG_PRE_(e->sig);   // the signal of this event
            QS_OBJ_PRE_(this);     // this queue object
            QS_2U8_PRE_(e->poolId_, e->refCtr_);// pool Id & refCtr of the evt
            QS_EQC_PRE_(nFree);    // number of free entries
            QS_EQC_PRE_(margin);   // margin requested
        QS_END_NOCRIT_PRE_()

        status = false; // event not posted
    }
    QF_CRIT_X_();

    static_cast<void>(qs_id); // unused parameter, if Q_SPY not defined

    return status;
}

//............................................................................
void QEQueue::postLIFO(QEvt const * const e,
                       std::uint_fast8_t const qs_id) noexcept
{
    static_cast<void>(qs_id); // unused parameter, if Q_SPY not defined

    QF_CRIT_STAT_
    QF_CRIT_E_();
    QEQueueCtr nFree = m_nFree; // temporary to avoid UB for volatile access

    //! @pre the queue must be able to accept the event (cannot overflow)
    Q_REQUIRE_CRIT_(300, nFree != 0U);

    // is it a dynamic event?
    if (e->poolId_ != 0U) {
        QF_EVT_REF_CTR_INC_(e); // increment the reference counter
    }

    --nFree; // one free entry just used up
    m_nFree = nFree; // update the volatile
    if (m_nMin > nFree) {
        m_nMin = nFree; // update minimum so far
    }

    QS_BEGIN_NOCRIT_PRE_(QS_QF_EQUEUE_POST_LIFO, qs_id)
        QS_TIME_PRE_();                      // timestamp
        QS_SIG_PRE_(e->sig);                 // the signal of this event
        QS_OBJ_PRE_(this);                   // this queue object
        QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool Id & refCtr of the evt
        QS_EQC_PRE_(nFree);                  // number of free entries
        QS_EQC_PRE_(m_nMin);                 // min number of free entries
    QS_END_NOCRIT_PRE_()

    QEvt const * const frontEvt = m_frontEvt; // read volatile into temporary
    m_frontEvt = e; // deliver event directly to the front of the queue

    // was the queue not empty?
    if (frontEvt != nullptr) {
        m_tail = (m_tail + 1U);
        if (m_tail == m_end) { // need to wrap the tail?
            m_tail = 0U; // wrap around
        }
        m_ring[m_tail] = frontEvt; // buffer the old front evt
    }
    QF_CRIT_X_();
}

//............................................................................
QEvt const *QEQueue::get(std::uint_fast8_t const qs_id) noexcept {
    static_cast<void>(qs_id); // unused parameter, if Q_SPY not defined

    QF_CRIT_STAT_
    QF_CRIT_E_();
    QEvt const * const e  = m_frontEvt; // always remove evt from the front

    // is the queue not empty?
    if (e != nullptr) {
        QEQueueCtr const nFree = m_nFree + 1U;
        m_nFree = nFree;  // upate the number of free

        // any events in the the ring buffer?
        if (nFree <= m_end) {
            m_frontEvt = m_ring[m_tail]; // remove from the tail
            if (m_tail == 0U) { // need to wrap?
                m_tail = m_end; // wrap around
            }
            m_tail = (m_tail - 1U);

            QS_BEGIN_NOCRIT_PRE_(QS_QF_EQUEUE_GET, qs_id)
                QS_TIME_PRE_();              // timestamp
                QS_SIG_PRE_(e->sig);         // the signal of this event
                QS_OBJ_PRE_(this);           // this queue object
                QS_2U8_PRE_(e->poolId_, e->refCtr_);
                QS_EQC_PRE_(nFree);          // # free entries
            QS_END_NOCRIT_PRE_()
        }
        else {
            m_frontEvt = nullptr; // queue becomes empty

            // all entries in the queue must be free (+1 for fronEvt)
            Q_ASSERT_CRIT_(410, nFree == (m_end + 1U));

            QS_BEGIN_NOCRIT_PRE_(QS_QF_EQUEUE_GET_LAST, qs_id)
                QS_TIME_PRE_();              // timestamp
                QS_SIG_PRE_(e->sig);         // the signal of this event
                QS_OBJ_PRE_(this);           // this queue object
                QS_2U8_PRE_(e->poolId_, e->refCtr_);
            QS_END_NOCRIT_PRE_()
        }
    }
    QF_CRIT_X_();

    return e;
}

} // namespace QP
