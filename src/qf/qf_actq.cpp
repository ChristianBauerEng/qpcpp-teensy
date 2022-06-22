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
//! @brief QP::QActive native queue operations (based on QP::QEQueue)
//!
//! @note
//! this source file is only included in the QF library when the native
//! QF active object queue is used (instead of a message queue of an RTOS).
//!

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

Q_DEFINE_THIS_MODULE("qf_actq")

} // unnamed namespace

//============================================================================
namespace QP {

#ifdef Q_SPY

//............................................................................
bool QActive::post_(QEvt const * const e,
                    std::uint_fast16_t const margin,
                    void const * const sender) noexcept
#else
bool QActive::post_(QEvt const * const e,
                    std::uint_fast16_t const margin) noexcept
#endif
{
    //! @pre event pointer must be valid
    Q_REQUIRE_ID(100, e != nullptr);

    QF_CRIT_STAT_
    QF_CRIT_E_();
    QEQueueCtr nFree = m_eQueue.m_nFree; // get volatile into the temporary

    // test-probe#1 for faking queue overflow
    QS_TEST_PROBE_DEF(&QActive::post_)
    QS_TEST_PROBE_ID(1,
        nFree = 0U;
    )

    bool status;
    if (margin == QF_NO_MARGIN) {
        if (nFree > 0U) {
            status = true; // can post
        }
        else {
            status = false; // cannot post
            Q_ERROR_CRIT_(110); // must be able to post the event
        }
    }
    else if (nFree > static_cast<QEQueueCtr>(margin)) {
        status = true; // can post
    }
    else {
        status = false; // cannot post, but don't assert
    }

    // is it a dynamic event?
    if (e->poolId_ != 0U) {
        QF_EVT_REF_CTR_INC_(e); // increment the reference counter
    }

    if (status) { // can post the event?

        --nFree;  // one free entry just used up
        m_eQueue.m_nFree = nFree; // update the volatile
        if (m_eQueue.m_nMin > nFree) {
            m_eQueue.m_nMin = nFree; // update minimum so far
        }

        QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_POST, m_prio)
            QS_TIME_PRE_();               // timestamp
            QS_OBJ_PRE_(sender);          // the sender object
            QS_SIG_PRE_(e->sig);          // the signal of the event
            QS_OBJ_PRE_(this);            // this active object
            QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool-Id & ref-ctr
            QS_EQC_PRE_(nFree);           // number of free entries
            QS_EQC_PRE_(m_eQueue.m_nMin); // min number of free entries
        QS_END_NOCRIT_PRE_()

#ifdef Q_UTEST
        // callback to examine the posted event under the same conditions
        // as producing the #QS_QF_ACTIVE_POST trace record, which are:
        // the local filter for this AO ('me->prio') is set
        //
        if (QS_LOC_CHECK_(m_prio)) {
            QS::onTestPost(sender, this, e, status);
        }
#endif
        // empty queue?
        if (m_eQueue.m_frontEvt == nullptr) {
            m_eQueue.m_frontEvt = e;      // deliver event directly
            QACTIVE_EQUEUE_SIGNAL_(this); // signal the event queue
        }
        // queue is not empty, insert event into the ring-buffer
        else {
            // insert event pointer e into the buffer (FIFO)
            m_eQueue.m_ring[m_eQueue.m_head] = e;

            // need to wrap head?
            if (m_eQueue.m_head == 0U) {
                m_eQueue.m_head = m_eQueue.m_end; // wrap around
            }
            // advance the head (counter clockwise)
            m_eQueue.m_head = (m_eQueue.m_head - 1U);
        }

        QF_CRIT_X_();
    }
    else { // cannot post the event

        QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_POST_ATTEMPT, m_prio)
            QS_TIME_PRE_();           // timestamp
            QS_OBJ_PRE_(sender);      // the sender object
            QS_SIG_PRE_(e->sig);      // the signal of the event
            QS_OBJ_PRE_(this);        // this active object
            QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool-Id & ref-ctr
            QS_EQC_PRE_(nFree);       // number of free entries
            QS_EQC_PRE_(margin);      // margin requested
        QS_END_NOCRIT_PRE_()

#ifdef Q_UTEST
        // callback to examine the posted event under the same conditions
        // as producing the #QS_QF_ACTIVE_POST trace record, which are:
        // the local filter for this AO ('me->prio') is set
        //
        if (QS_LOC_CHECK_(m_prio)) {
            QS::onTestPost(sender, this, e, status);
        }
#endif

        QF_CRIT_X_();

        QF::gc(e); // recycle the event to avoid a leak
    }

    return status;
}

//............................................................................
void QActive::postLIFO(QEvt const * const e) noexcept {
    QF_CRIT_STAT_
    QF_CRIT_E_();
    QEQueueCtr nFree = m_eQueue.m_nFree;// tmp to avoid UB for volatile access

    QS_TEST_PROBE_DEF(&QActive::postLIFO)
    QS_TEST_PROBE_ID(1,
        nFree = 0U;
    )

    // the queue must be able to accept the event (cannot overflow)
    Q_ASSERT_CRIT_(210, nFree != 0U);

    // is it a dynamic event?
    if (e->poolId_ != 0U) {
        QF_EVT_REF_CTR_INC_(e); // increment the reference counter
    }

    --nFree;  // one free entry just used up
    m_eQueue.m_nFree = nFree; // update the volatile
    if (m_eQueue.m_nMin > nFree) {
        m_eQueue.m_nMin = nFree; // update minimum so far
    }

    QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_POST_LIFO, m_prio)
        QS_TIME_PRE_();                      // timestamp
        QS_SIG_PRE_(e->sig);                 // the signal of this event
        QS_OBJ_PRE_(this);                   // this active object
        QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool-Id & ref-ctr
        QS_EQC_PRE_(nFree);                  // number of free entries
        QS_EQC_PRE_(m_eQueue.m_nMin);        // min number of free entries
    QS_END_NOCRIT_PRE_()

#ifdef Q_UTEST
    // callback to examine the posted event under the same conditions
    // as producing the #QS_QF_ACTIVE_POST trace record, which are:
    // the local filter for this AO ('me->prio') is set
    //
    if (QS_LOC_CHECK_(m_prio)) {
        QS::onTestPost(nullptr, this, e, true);
    }
#endif

    // read volatile into temporary
    QEvt const * const frontEvt = m_eQueue.m_frontEvt;
    m_eQueue.m_frontEvt = e; // deliver the event directly to the front

    // was the queue empty?
    if (frontEvt == nullptr) {
        QACTIVE_EQUEUE_SIGNAL_(this); // signal the event queue
    }
    // queue was not empty, leave the event in the ring-buffer
    else {
        m_eQueue.m_tail = (m_eQueue.m_tail + 1U);
        if (m_eQueue.m_tail == m_eQueue.m_end) { // need to wrap the tail?
            m_eQueue.m_tail = 0U; // wrap around
        }

        m_eQueue.m_ring[m_eQueue.m_tail] = frontEvt;
    }
    QF_CRIT_X_();
}

//............................................................................
QEvt const *QActive::get_(void) noexcept {

    QF_CRIT_STAT_
    QF_CRIT_E_();
    QACTIVE_EQUEUE_WAIT_(this); // wait for event to arrive directly

    // always remove evt from the front
    QEvt const * const e = m_eQueue.m_frontEvt;
    QEQueueCtr const nFree = m_eQueue.m_nFree + 1U;
    m_eQueue.m_nFree = nFree; // upate the number of free

    // any events in the ring buffer?
    if (nFree <= m_eQueue.m_end) {

        // remove event from the tail
        m_eQueue.m_frontEvt = m_eQueue.m_ring[m_eQueue.m_tail];
        if (m_eQueue.m_tail == 0U) { // need to wrap?
            m_eQueue.m_tail = m_eQueue.m_end; // wrap around
        }
        m_eQueue.m_tail = (m_eQueue.m_tail - 1U);

        QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_GET, m_prio)
            QS_TIME_PRE_();                      // timestamp
            QS_SIG_PRE_(e->sig);                 // the signal of this event
            QS_OBJ_PRE_(this);                   // this active object
            QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool-Id & ref-ctr
            QS_EQC_PRE_(nFree);                  // number of free entries
        QS_END_NOCRIT_PRE_()
    }
    else {
        // the queue becomes empty
        m_eQueue.m_frontEvt = nullptr;

        // all entries in the queue must be free (+1 for fronEvt)
        Q_ASSERT_CRIT_(310, nFree == (m_eQueue.m_end + 1U));

        QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_GET_LAST, m_prio)
            QS_TIME_PRE_();                      // timestamp
            QS_SIG_PRE_(e->sig);                 // the signal of this event
            QS_OBJ_PRE_(this);                   // this active object
            QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool-Id & ref-ctr
        QS_END_NOCRIT_PRE_()
    }
    QF_CRIT_X_();
    return e;
}

//............................................................................
std::uint_fast16_t QF::getQueueMin(std::uint_fast8_t const prio) noexcept {

    Q_REQUIRE_ID(400, (prio <= QF_MAX_ACTIVE)
                      && (active_[prio] != nullptr));

    QF_CRIT_STAT_
    QF_CRIT_E_();
    std::uint_fast16_t const min =
        static_cast<std::uint_fast16_t>(active_[prio]->m_eQueue.m_nMin);
    QF_CRIT_X_();

    return min;
}

//============================================================================
QTicker::QTicker(std::uint_fast8_t const tickRate) noexcept
  : QActive(nullptr)
{
    // reuse m_head for tick-rate
    m_eQueue.m_head = static_cast<QEQueueCtr>(tickRate);
}
//............................................................................
void QTicker::init(void const * const e,
                   std::uint_fast8_t const qs_id) noexcept
{
    static_cast<void>(e); // unused parameter
    static_cast<void>(qs_id); // unused parameter
    m_eQueue.m_tail = 0U;
}
//............................................................................
void QTicker::dispatch(QEvt const * const e,
                   std::uint_fast8_t const qs_id) noexcept
{
    static_cast<void>(e); // unused parameter
    static_cast<void>(qs_id); // unused parameter

    QF_CRIT_STAT_
    QF_CRIT_E_();
    QEQueueCtr nTicks = m_eQueue.m_tail; // # ticks since the last call
    m_eQueue.m_tail = 0U; // clear the # ticks
    QF_CRIT_X_();

    for (; nTicks > 0U; --nTicks) {
        QF::TICK_X(static_cast<std::uint_fast8_t>(m_eQueue.m_head), this);
    }
}
//............................................................................
void QTicker::init(std::uint_fast8_t const qs_id) noexcept {
    QTicker::init(nullptr, qs_id);
}

//============================================================================
#ifdef Q_SPY

//............................................................................
bool QTicker::post_(QEvt const * const e, std::uint_fast16_t const margin,
                    void const * const sender) noexcept
#else
bool QTicker::post_(QEvt const * const e, std::uint_fast16_t const margin)
    noexcept
#endif
{
    static_cast<void>(e);      // unused parameter
    static_cast<void>(margin); // unused parameter

    QF_CRIT_STAT_
    QF_CRIT_E_();
    if (m_eQueue.m_frontEvt == nullptr) {

#ifdef Q_EVT_CTOR
        static QEvt const tickEvt(0U, QEvt::STATIC_EVT);
#else
        static QEvt const tickEvt = { 0U, 0U, 0U };
#endif // Q_EVT_CTOR

        m_eQueue.m_frontEvt = &tickEvt; // deliver event directly
        m_eQueue.m_nFree = (m_eQueue.m_nFree - 1U); // one less free event

        QACTIVE_EQUEUE_SIGNAL_(this); // signal the event queue
    }

    // account for one more tick event
    m_eQueue.m_tail = (m_eQueue.m_tail + 1U);

    QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_POST, m_prio)
        QS_TIME_PRE_();      // timestamp
        QS_OBJ_PRE_(sender); // the sender object
        QS_SIG_PRE_(0U);     // the signal of the event
        QS_OBJ_PRE_(this);   // this active object
        QS_2U8_PRE_(0U, 0U); // pool-Id & ref-ctr
        QS_EQC_PRE_(0U);     // number of free entries
        QS_EQC_PRE_(0U);     // min number of free entries
    QS_END_NOCRIT_PRE_()

    QF_CRIT_X_();

    return true; // the event is always posted correctly
}

//............................................................................
void QTicker::postLIFO(QEvt const * const e) noexcept {
    static_cast<void>(e); // unused parameter
    Q_ERROR_ID(900); // operation not allowed
}

} // namespace QP
