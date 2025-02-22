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
//! @date Last updated on: 2021-12-23
//! @version Last updated for: @ref qpcpp_7_0_0
//!
//! @file
//! @brief QF/C++ Publish-Subscribe services
//! definitions.

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

Q_DEFINE_THIS_MODULE("qf_ps")

} // unnamed namespace

namespace QP {

// Package-scope objects *****************************************************
QSubscrList *QF_subscrList_;
enum_t QF_maxPubSignal_;

//============================================================================
//! @description
//! This function initializes the publish-subscribe facilities of QF and must
//! be called exactly once before any subscriptions/publications occur in
//! the application.
//!
//! @param[in] subscrSto pointer to the array of subscriber lists
//! @param[in] maxSignal the dimension of the subscriber array and at
//!                      the same time the maximum signal that can be
//!                      published or subscribed.
//!
//! The array of subscriber-lists is indexed by signals and provides a mapping
//! between the signals and subscriber-lists. The subscriber-lists are
//! bitmasks of type QP::QSubscrList, each bit in the bit mask corresponding
//! to the unique priority of an active object. The size of the
//! QP::QSubscrList bitmask depends on the value of the #QF_MAX_ACTIVE macro.
//!
//! @note
//! The publish-subscribe facilities are optional, meaning that you might
//! choose not to use publish-subscribe. In that case calling QF::psInit()
//! and using up memory for the subscriber-lists is unnecessary.
//!
//! @sa
//! QP::QSubscrList
//!
//! @usage
//! The following example shows the typical initialization sequence of QF:
//! @include qf_main.cpp
//!
void QF::psInit(QSubscrList * const subscrSto,
                enum_t const maxSignal) noexcept
{
    QF_subscrList_   = subscrSto;
    QF_maxPubSignal_ = maxSignal;

    // zero the subscriber list, so that the framework can start correctly
    // even if the startup code fails to clear the uninitialized data
    // (as is required by the C++ Standard)
    bzero(subscrSto, static_cast<unsigned>(maxSignal) * sizeof(QSubscrList));
}

//============================================================================
//! @description
//! This function posts (using the FIFO policy) the event @a e to **all**
//! active objects that have subscribed to the signal @a e->sig, which is
//! called _multicasting_. The multicasting performed in this function is
//! very efficient based on reference-counting inside the published event
//! ("zero-copy" event multicasting). This function is designed to be
//! callable from any part of the system, including ISRs, device drivers,
//! and active objects.
//!
//! @note
//! To avoid any unexpected re-ordering of events posted into AO queues,
//! the event multicasting is performed with scheduler __locked__. However,
//! the scheduler is locked only up to the priority level of the highest-
//! priority subscriber, so any AOs of even higher priority, which did not
//! subscribe to this event are _not_ affected.
//!
#ifndef Q_SPY
void QF::publish_(QEvt const * const e) noexcept {
#else
void QF::publish_(QEvt const * const e,
                  void const * const sender,
                  std::uint_fast8_t const qs_id) noexcept
{
#endif
    //! @pre the published signal must be within the configured range
    Q_REQUIRE_ID(100, static_cast<enum_t>(e->sig) < QF_maxPubSignal_);

    QF_CRIT_STAT_
    QF_CRIT_E_();

    QS_BEGIN_NOCRIT_PRE_(QS_QF_PUBLISH, qs_id)
        QS_TIME_PRE_();                      // the timestamp
        QS_OBJ_PRE_(sender);                 // the sender object
        QS_SIG_PRE_(e->sig);                 // the signal of the event
        QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool Id & refCtr of the evt
    QS_END_NOCRIT_PRE_()

    // is it a dynamic event?
    if (e->poolId_ != 0U) {
        // NOTE: The reference counter of a dynamic event is incremented to
        // prevent premature recycling of the event while the multicasting
        // is still in progress. At the end of the function, the garbage
        // collector step (QF::gc()) decrements the reference counter and
        // recycles the event if the counter drops to zero. This covers the
        // case when the event was published without any subscribers.
        //
        QF_EVT_REF_CTR_INC_(e);
    }

    // make a local, modifiable copy of the subscriber list
    QPSet subscrList = QF_subscrList_[e->sig];
    QF_CRIT_X_();

    if (subscrList.notEmpty()) { // any subscribers?
        // the highest-prio subscriber
        std::uint_fast8_t p = subscrList.findMax();
        QF_SCHED_STAT_

        QF_SCHED_LOCK_(p); // lock the scheduler up to prio 'p'
        do { // loop over all subscribers */
            // the prio of the AO must be registered with the framework
            Q_ASSERT_ID(210, active_[p] != nullptr);

            // POST() asserts internally if the queue overflows
            static_cast<void>(active_[p]->POST(e, sender));

            subscrList.rmove(p); // remove the handled subscriber
            if (subscrList.notEmpty()) {  // still more subscribers?
                p = subscrList.findMax(); // the highest-prio subscriber
            }
            else {
                p = 0U; // no more subscribers
            }
        } while (p != 0U);
        QF_SCHED_UNLOCK_(); // unlock the scheduler
    }

    // The following garbage collection step decrements the reference counter
    // and recycles the event if the counter drops to zero. This covers both
    // cases when the event was published with or without any subscribers.
    //
    gc(e);
}


//============================================================================
//! @description
//! This function is part of the Publish-Subscribe event delivery mechanism
//! available in QF. Subscribing to an event means that the framework will
//! start posting all published events with a given signal @p sig to the
//! event queue of the active object.
//!
//! @param[in] sig event signal to subscribe
//!
//! The following example shows how the Table active object subscribes
//! to three signals in the initial transition:
//! @include qf_subscribe.cpp
//!
//! @sa
//! QP::QF::publish_(), QP::QActive::unsubscribe(), and
//! QP::QActive::unsubscribeAll()
//!
void QActive::subscribe(enum_t const sig) const noexcept {
    std::uint_fast8_t const p = static_cast<std::uint_fast8_t>(m_prio);

    Q_REQUIRE_ID(300, (Q_USER_SIG <= sig)
              && (sig < QF_maxPubSignal_)
              && (0U < p) && (p <= QF_MAX_ACTIVE)
              && (QF::active_[p] == this));

    QF_CRIT_STAT_
    QF_CRIT_E_();

    QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_SUBSCRIBE, m_prio)
        QS_TIME_PRE_();    // timestamp
        QS_SIG_PRE_(sig);  // the signal of this event
        QS_OBJ_PRE_(this); // this active object
    QS_END_NOCRIT_PRE_()

    QF_subscrList_[sig].insert(p); // insert into subscriber-list
    QF_CRIT_X_();
}

//============================================================================
//! @description
//! This function is part of the Publish-Subscribe event delivery mechanism
//! available in QF. Un-subscribing from an event means that the framework
//! will stop posting published events with a given signal @p sig to the
//! event queue of the active object.
//!
//! @param[in] sig event signal to unsubscribe
//!
//! @note
//! Due to the latency of event queues, an active object should NOT
//! assume that a given signal @p sig will never be dispatched to the
//! state machine of the active object after un-subscribing from that signal.
//! The event might be already in the queue, or just about to be posted
//! and the un-subscribe operation will not flush such events.
//!
//! @note
//! Un-subscribing from a signal that has never been subscribed in the
//! first place is considered an error and QF will raise an assertion.
//!
//! @sa
//! QP::QF::publish_(), QP::QActive::subscribe(), and
//! QP::QActive::unsubscribeAll()
//!
void QActive::unsubscribe(enum_t const sig) const noexcept {
    std::uint_fast8_t const p = static_cast<std::uint_fast8_t>(m_prio);

    //! @pre the singal and the prioriy must be in ragne, the AO must also
    // be registered with the framework
    Q_REQUIRE_ID(400, (Q_USER_SIG <= sig)
                      && (sig < QF_maxPubSignal_)
                      && (0U < p) && (p <= QF_MAX_ACTIVE)
                      && (QF::active_[p] == this));

    QF_CRIT_STAT_
    QF_CRIT_E_();

    QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_UNSUBSCRIBE, m_prio)
        QS_TIME_PRE_();         // timestamp
        QS_SIG_PRE_(sig);       // the signal of this event
        QS_OBJ_PRE_(this);      // this active object
    QS_END_NOCRIT_PRE_()

    QF_subscrList_[sig].rmove(p); // remove from subscriber-list

    QF_CRIT_X_();
}

//============================================================================
//! @description
//! This function is part of the Publish-Subscribe event delivery mechanism
//! available in QF. Un-subscribing from all events means that the framework
//! will stop posting any published events to the event queue of the active
//! object.
//!
//! @note
//! Due to the latency of event queues, an active object should NOT
//! assume that no events will ever be dispatched to the state machine of
//! the active object after un-subscribing from all events.
//! The events might be already in the queue, or just about to be posted
//! and the un-subscribe operation will not flush such events. Also, the
//! alternative event-delivery mechanisms, such as direct event posting or
//! time events, can be still delivered to the event queue of the active
//! object.
//!
//! @sa
//! QP::QF::publish_(), QP::QActive::subscribe(), and
//! QP::QActive::unsubscribe()
//!
void QActive::unsubscribeAll(void) const noexcept {
    std::uint_fast8_t const p = static_cast<std::uint_fast8_t>(m_prio);

    Q_REQUIRE_ID(500, (0U < p) && (p <= QF_MAX_ACTIVE)
                      && (QF::active_[p] == this));

    for (enum_t sig = Q_USER_SIG; sig < QF_maxPubSignal_; ++sig) {
        QF_CRIT_STAT_
        QF_CRIT_E_();
        if (QF_subscrList_[sig].hasElement(p)) {
            QF_subscrList_[sig].rmove(p);

            QS_BEGIN_NOCRIT_PRE_(QS_QF_ACTIVE_UNSUBSCRIBE, m_prio)
                QS_TIME_PRE_();     // timestamp
                QS_SIG_PRE_(sig);   // the signal of this event
                QS_OBJ_PRE_(this);  // this active object
            QS_END_NOCRIT_PRE_()

        }
        QF_CRIT_X_();

        // prevent merging critical sections
        QF_CRIT_EXIT_NOP();
    }
}

} // namespace QP
