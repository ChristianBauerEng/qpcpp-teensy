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
//! @brief QP::QHsm implementation
//!
//! @tr{RQP103} @tr{RQP104} @tr{RQP120} @tr{RQP130}

#define QP_IMPL             // this is QP implementation
#include "qep_port.hpp"     // QEP port
#ifdef Q_SPY                // QS software tracing enabled?
    #include "qs_port.hpp"  // QS port
    #include "qs_pkg.hpp"   // QS facilities for pre-defined trace records
#else
    #include "qs_dummy.hpp" // disable the QS software tracing
#endif // Q_SPY
#include "qassert.h"        // QP embedded systems-friendly assertions

// local helper macros...

//! helper macro to trigger internal event in an HSM
#define QEP_TRIG_(state_, sig_) \
    ((*(state_))(this, &QEP_reservedEvt_[sig_]))

//! helper macro to trigger exit action in an HSM
#define QEP_EXIT_(state_) do {                            \
    if (QEP_TRIG_(state_, Q_EXIT_SIG) == Q_RET_HANDLED) { \
        QS_BEGIN_PRE_(QS_QEP_STATE_EXIT, qs_id)           \
            QS_OBJ_PRE_(this);                            \
            QS_FUN_PRE_(state_);                          \
        QS_END_PRE_() \
    } \
} while (false)

//! helper macro to trigger entry action in an HSM
#define QEP_ENTER_(state_) do { \
    if (QEP_TRIG_(state_, Q_ENTRY_SIG) == Q_RET_HANDLED) { \
        QS_BEGIN_PRE_(QS_QEP_STATE_ENTRY, qs_id)           \
            QS_OBJ_PRE_(this);                             \
            QS_FUN_PRE_(state_);                           \
        QS_END_PRE_() \
    } \
} while (false)

// unnamed namespace for local definitions with internal linkage
namespace {

Q_DEFINE_THIS_MODULE("qep_hsm")

//============================================================================
enum : QP::QSignal {
    //! empty signal for internal use only
    QEP_EMPTY_SIG_ = 0U
};

//============================================================================
//! @description
//! Static, preallocated standard events that the QEP event processor sends
//! to state handler functions of QP::QHsm-style state machine to execute entry
//! actions, exit actions, and initial transitions.
//!
static QP::QEvt const QEP_reservedEvt_[4] {
#ifdef Q_EVT_CTOR // Is the QEvt constructor provided?
    QP::QEvt(0U, QP::QEvt::STATIC_EVT),
    QP::QEvt(1U, QP::QEvt::STATIC_EVT),
    QP::QEvt(2U, QP::QEvt::STATIC_EVT),
    QP::QEvt(3U, QP::QEvt::STATIC_EVT)
#else // QEvt is a POD (Plain Old Datatype)
    { 0U, 0U, 0U },
    { 1U, 0U, 0U },
    { 2U, 0U, 0U },
    { 3U, 0U, 0U }
#endif
};

} // unnamed namespace

namespace QP {

//============================================================================
//char const versionStr[7] = QP_VERSION_STR;

//============================================================================
//! @description
//! Performs the first step of HSM initialization by assigning the initial
//! pseudostate to the currently active state of the state machine.
//!
//! @param[in] initial pointer to the top-most initial state-handler
//!                    function in the derived state machine
//! @tr{RQP103}
//!
QHsm::QHsm(QStateHandler const initial) noexcept {
    m_state.fun = Q_STATE_CAST(&top);
    m_temp.fun  = initial;
}

//============================================================================
//! @description
//! Virtual destructor of the QHsm state machine and any of its subclasses.
//!
QHsm::~QHsm() {
}

//============================================================================
//! @description
//! Executes the top-most initial transition in a HSM.
//!
//! @param[in] e   pointer to an extra parameter (might be NULL)
//! @param[in] qs_id QS-id of this state machine (for QS local filter)
//!
//! @note
//! Must be called exactly __once__ before the QP::QHsm::dispatch().
//!
//! @tr{RQP103} @tr{RQP120I} @tr{RQP120D}
//!
void QHsm::init(void const * const e, std::uint_fast8_t const qs_id) {
    static_cast<void>(qs_id); // unused parameter (if Q_SPY not defined)

    QStateHandler t = m_state.fun;

    //! @pre ctor must have been executed and initial tran NOT taken
    Q_REQUIRE_ID(200, (m_temp.fun != nullptr)
                      && (t == Q_STATE_CAST(&top)));

    // execute the top-most initial transition
    QState r = (*m_temp.fun)(this, Q_EVT_CAST(QEvt));

    // the top-most initial transition must be taken
    Q_ASSERT_ID(210, r == Q_RET_TRAN);

    QS_CRIT_STAT_
    QS_BEGIN_PRE_(QS_QEP_STATE_INIT, qs_id)
        QS_OBJ_PRE_(this);       // this state machine object
        QS_FUN_PRE_(t);          // the source state
        QS_FUN_PRE_(m_temp.fun); // the target of the initial transition
    QS_END_PRE_()

    // drill down into the state hierarchy with initial transitions...
    do {
        QStateHandler path[MAX_NEST_DEPTH_]; // tran entry path array
        std::int_fast8_t ip = 0; // entry path index

        path[0] = m_temp.fun;
        static_cast<void>(QEP_TRIG_(m_temp.fun, QEP_EMPTY_SIG_));
        while (m_temp.fun != t) {
            ++ip;
            Q_ASSERT_ID(220, ip < MAX_NEST_DEPTH_);
            path[ip] = m_temp.fun;
            static_cast<void>(QEP_TRIG_(m_temp.fun, QEP_EMPTY_SIG_));
        }
        m_temp.fun = path[0];

        // retrace the entry path in reverse (desired) order...
        do {
            QEP_ENTER_(path[ip]); // enter path[ip]
            --ip;
        } while (ip >= 0);

        t = path[0]; // current state becomes the new source

        r = QEP_TRIG_(t, Q_INIT_SIG); // execute initial transition

#ifdef Q_SPY
        if (r == Q_RET_TRAN) {
            QS_BEGIN_PRE_(QS_QEP_STATE_INIT, qs_id)
                QS_OBJ_PRE_(this);       // this state machine object
                QS_FUN_PRE_(t);          // the source state
                QS_FUN_PRE_(m_temp.fun); // the target of the initial tran.
            QS_END_PRE_()
        }
#endif // Q_SPY

    } while (r == Q_RET_TRAN);

    QS_BEGIN_PRE_(QS_QEP_INIT_TRAN, qs_id)
        QS_TIME_PRE_();    // time stamp
        QS_OBJ_PRE_(this); // this state machine object
        QS_FUN_PRE_(t);    // the new active state
    QS_END_PRE_()

    m_state.fun = t; // change the current active state
    m_temp.fun  = t; // mark the configuration as stable
}

//***************************************************************************/
//! @description
//! The QP::QHsm::top() state handler is the ultimate root of state hierarchy
//! in all HSMs derived from QP::QHsm.
//!
//! @param[in] me pointer to the HSM instance
//! @param[in] e  pointer to the event to be dispatched to the HSM
//!
//! @returns
//! Always returns #Q_RET_IGNORED, which means that the top state ignores
//! all events.
//!
//! @note
//! The parameters to this state handler are not used. They are provided for
//! conformance with the state-handler function signature QP::QStateHandler.
//!
//!
//! @tr{RQP103} @tr{RQP120T}
//!
QState QHsm::top(void * const me, QEvt const * const e) noexcept {
    static_cast<void>(me); // unused parameter
    static_cast<void>(e);  // unused parameter
    return Q_RET_IGNORED; // the top state ignores all events
}

//============================================================================
//! @description
//! Dispatches an event for processing to a hierarchical state machine (HSM).
//! The processing of an event represents one run-to-completion (RTC) step.
//!
//! @param[in] e  pointer to the event to be dispatched to the HSM
//! @param[in] qs_id QS-id of this state machine (for QS local filter)
//!
//! @note
//! This state machine must be initialized by calling QP::QHsm::init() exactly
//! __once__ before calling QP::QHsm::dispatch().
//!
//! @tr{RQP103}
//! @tr{RQP120A} @tr{RQP120B} @tr{RQP120C} @tr{RQP120D} @tr{RQP120E}
//!
void QHsm::dispatch(QEvt const * const e, std::uint_fast8_t const qs_id) {
    QStateHandler t = m_state.fun;
    QS_CRIT_STAT_

    //! @pre the current state must be initialized and
    //! the state configuration must be stable
    Q_REQUIRE_ID(400, (t != nullptr)
                       && (t == m_temp.fun));

    QS_BEGIN_PRE_(QS_QEP_DISPATCH, qs_id)
        QS_TIME_PRE_();         // time stamp
        QS_SIG_PRE_(e->sig);    // the signal of the event
        QS_OBJ_PRE_(this);      // this state machine object
        QS_FUN_PRE_(t);         // the current state
    QS_END_PRE_()

    QStateHandler s;
    QState r;
    // process the event hierarchically...
    //! @tr{RQP120A}
    do {
        s = m_temp.fun;
        r = (*s)(this, e); // invoke state handler s

        if (r == Q_RET_UNHANDLED) { // unhandled due to a guard?

            QS_BEGIN_PRE_(QS_QEP_UNHANDLED, qs_id)
                QS_SIG_PRE_(e->sig); // the signal of the event
                QS_OBJ_PRE_(this);   // this state machine object
                QS_FUN_PRE_(s);      // the current state
            QS_END_PRE_()

            r = QEP_TRIG_(s, QEP_EMPTY_SIG_); // find superstate of s
        }
    } while (r == Q_RET_SUPER);

    // regular transition taken?
    //! @tr{RQP120E}
    if (r >= Q_RET_TRAN) {
        QStateHandler path[MAX_NEST_DEPTH_];

        path[0] = m_temp.fun; // save the target of the transition
        path[1] = t;
        path[2] = s;

        // exit current state to transition source s...
        //! @tr{RQP120C}
        for (; t != s; t = m_temp.fun) {
            // exit handled?
            if (QEP_TRIG_(t, Q_EXIT_SIG) == Q_RET_HANDLED) {
                QS_BEGIN_PRE_(QS_QEP_STATE_EXIT, qs_id)
                    QS_OBJ_PRE_(this); // this state machine object
                    QS_FUN_PRE_(t);    // the exited state
                QS_END_PRE_()

                // find superstate of t
                static_cast<void>(QEP_TRIG_(t, QEP_EMPTY_SIG_));
            }
        }

        std::int_fast8_t ip = hsm_tran(path, qs_id); // the HSM transition

#ifdef Q_SPY
        if (r == Q_RET_TRAN_HIST) {

            QS_BEGIN_PRE_(QS_QEP_TRAN_HIST, qs_id)
                QS_OBJ_PRE_(this);     // this state machine object
                QS_FUN_PRE_(t);        // the source of the transition
                QS_FUN_PRE_(path[0]);  // the target of the tran. to history
            QS_END_PRE_()

        }
#endif // Q_SPY

        // execute state entry actions in the desired order...
        //! @tr{RQP120B}
        for (; ip >= 0; --ip) {
            QEP_ENTER_(path[ip]); // enter path[ip]
        }
        t = path[0];    // stick the target into register
        m_temp.fun = t; // update the next state

        // drill into the target hierarchy...
        //! @tr{RQP120I}
        while (QEP_TRIG_(t, Q_INIT_SIG) == Q_RET_TRAN) {

            QS_BEGIN_PRE_(QS_QEP_STATE_INIT, qs_id)
                QS_OBJ_PRE_(this);       // this state machine object
                QS_FUN_PRE_(t);          // the source (pseudo)state
                QS_FUN_PRE_(m_temp.fun); // the target of the transition
            QS_END_PRE_()

            ip = 0;
            path[0] = m_temp.fun;

            // find superstate
            static_cast<void>(QEP_TRIG_(m_temp.fun, QEP_EMPTY_SIG_));

            while (m_temp.fun != t) {
                ++ip;
                path[ip] = m_temp.fun;
                // find superstate
                static_cast<void>(QEP_TRIG_(m_temp.fun, QEP_EMPTY_SIG_));
            }
            m_temp.fun = path[0];

            // entry path must not overflow
            Q_ASSERT_ID(410, ip < MAX_NEST_DEPTH_);

            // retrace the entry path in reverse (correct) order...
            do {
                QEP_ENTER_(path[ip]);  // enter path[ip]
                --ip;
            } while (ip >= 0);

            t = path[0];
        }

        QS_BEGIN_PRE_(QS_QEP_TRAN, qs_id)
            QS_TIME_PRE_();          // time stamp
            QS_SIG_PRE_(e->sig);     // the signal of the event
            QS_OBJ_PRE_(this);       // this state machine object
            QS_FUN_PRE_(s);          // the source of the transition
            QS_FUN_PRE_(t);          // the new active state
        QS_END_PRE_()
    }

#ifdef Q_SPY
    else if (r == Q_RET_HANDLED) {

        QS_BEGIN_PRE_(QS_QEP_INTERN_TRAN, qs_id)
            QS_TIME_PRE_();          // time stamp
            QS_SIG_PRE_(e->sig);     // the signal of the event
            QS_OBJ_PRE_(this);       // this state machine object
            QS_FUN_PRE_(s);          // the source state
        QS_END_PRE_()

    }
    else {

        QS_BEGIN_PRE_(QS_QEP_IGNORED, qs_id)
            QS_TIME_PRE_();          // time stamp
            QS_SIG_PRE_(e->sig);     // the signal of the event
            QS_OBJ_PRE_(this);       // this state machine object
            QS_FUN_PRE_(m_state.fun);// the current state
        QS_END_PRE_()

    }
#endif // Q_SPY

    m_state.fun = t; // change the current active state
    m_temp.fun  = t; // mark the configuration as stable
    static_cast<void>(qs_id); // unused parameter (if Q_SPY not defined)
}

//============================================================================
//! @description
//! helper function to execute transition sequence in a hierarchical state
//! machine (HSM).
//!
//! @param[in,out] path array of pointers to state-handler functions
//!                     to execute the entry actions
//! @param[in]     qs_id QS-id of this state machine (for QS local filter)
//!
//! @returns
//! the depth of the entry path stored in the @p path parameter.
//!
//! @tr{RQP103}
//! @tr{RQP120E} @tr{RQP120F}
//!
std::int_fast8_t QHsm::hsm_tran(QStateHandler (&path)[MAX_NEST_DEPTH_],
                                std::uint_fast8_t const qs_id)
{
    std::int_fast8_t ip = -1; // transition entry path index
    QStateHandler t = path[0];
    QStateHandler const s = path[2];
    QS_CRIT_STAT_

    // (a) check source==target (transition to self)
    if (s == t) {
        QEP_EXIT_(s);  // exit the source
        ip = 0; // enter the target
    }
    else {
        // superstate of target
        static_cast<void>(QEP_TRIG_(t, QEP_EMPTY_SIG_));
        t = m_temp.fun;

        // (b) check source==target->super
        if (s == t) {
            ip = 0; // enter the target
        }
        else {
            // superstate of src
            static_cast<void>(QEP_TRIG_(s, QEP_EMPTY_SIG_));

            // (c) check source->super==target->super
            if (m_temp.fun == t) {
                QEP_EXIT_(s);  // exit the source
                ip = 0; // enter the target
            }
            else {
                // (d) check source->super==target
                if (m_temp.fun == path[0]) {
                    QEP_EXIT_(s); // exit the source
                }
                else {
                    // (e) check rest of source==target->super->super..
                    // and store the entry path along the way
                    std::int_fast8_t iq = 0; // indicate that LCA was found
                    ip = 1; // enter target and its superstate
                    path[1] = t; // save the superstate of target
                    t = m_temp.fun; // save source->super

                    // find target->super->super
                    QState r = QEP_TRIG_(path[1], QEP_EMPTY_SIG_);
                    while (r == Q_RET_SUPER) {
                        ++ip;
                        path[ip] = m_temp.fun; // store the entry path
                        if (m_temp.fun == s) { // is it the source?
                            // indicate that the LCA was found
                            iq = 1;

                            // entry path must not overflow
                            Q_ASSERT_ID(510, ip < MAX_NEST_DEPTH_);
                            --ip;  // do not enter the source
                            r = Q_RET_HANDLED; // terminate the loop
                        }
                        // it is not the source, keep going up
                        else {
                            r = QEP_TRIG_(m_temp.fun, QEP_EMPTY_SIG_);
                        }
                    }

                    // the LCA not found yet?
                    if (iq == 0) {
                        // entry path must not overflow
                        Q_ASSERT_ID(520, ip < MAX_NEST_DEPTH_);

                        QEP_EXIT_(s); // exit the source

                        // (f) check the rest of source->super
                        //                  == target->super->super...
                        //
                        iq = ip;
                        r = Q_RET_IGNORED; // indicate LCA NOT found
                        do {
                            // is this the LCA?
                            if (t == path[iq]) {
                                r = Q_RET_HANDLED; // indicate LCA found
                                ip = iq - 1; // do not enter LCA
                                iq = -1; // cause termination of the loop
                            }
                            else {
                                --iq; // try lower superstate of target
                            }
                        } while (iq >= 0);

                        // LCA not found yet?
                        if (r != Q_RET_HANDLED) {
                            // (g) check each source->super->...
                            // for each target->super...
                            //
                            r = Q_RET_IGNORED; // keep looping
                            do {
                                // exit t unhandled?
                                if (QEP_TRIG_(t, Q_EXIT_SIG) == Q_RET_HANDLED)
                                {
                                    QS_BEGIN_PRE_(QS_QEP_STATE_EXIT, qs_id)
                                        QS_OBJ_PRE_(this);
                                        QS_FUN_PRE_(t);
                                    QS_END_PRE_()

                                    static_cast<void>(
                                        QEP_TRIG_(t, QEP_EMPTY_SIG_));
                                }
                                t = m_temp.fun; //  set to super of t
                                iq = ip;
                                do {
                                    // is this LCA?
                                    if (t == path[iq]) {
                                        ip = iq - 1; // do not enter LCA
                                        iq = -1; // break out of inner loop
                                        r = Q_RET_HANDLED; // break outer loop
                                    }
                                    else {
                                        --iq;
                                    }
                                } while (iq >= 0);
                            } while (r != Q_RET_HANDLED);
                        }
                    }
                }
            }
        }
    }
    static_cast<void>(qs_id); // unused parameter (if Q_SPY not defined)
    return ip;
}

//============================================================================
#ifdef Q_SPY
    QStateHandler QHsm::getStateHandler() noexcept {
        return m_state.fun;
    }
#endif

//============================================================================
//! @description
//! Tests if a state machine derived from QHsm is-in a given state.
//!
//! @note
//! For a HSM, to "be in a state" means also to be in a superstate of
//! of the state.
//!
//! @param[in] s pointer to the state-handler function to be tested
//!
//! @returns
//! 'true' if the HSM is in the @p state and 'false' otherwise
//!
//! @tr{RQP103}
//! @tr{RQP120S}
//!
bool QHsm::isIn(QStateHandler const s) noexcept {

    //! @pre state configuration must be stable
    Q_REQUIRE_ID(600, m_temp.fun == m_state.fun);

    bool inState = false;  // assume that this HSM is not in 'state'

    // scan the state hierarchy bottom-up
    QState r;
    do {
        // do the states match?
        if (m_temp.fun == s) {
            inState = true;    // 'true' means that match found
            r = Q_RET_IGNORED; // cause breaking out of the loop
        }
        else {
            r = QEP_TRIG_(m_temp.fun, QEP_EMPTY_SIG_);
        }
    } while (r != Q_RET_IGNORED); // QHsm::top() state not reached
    m_temp.fun = m_state.fun; // restore the stable state configuration

    return inState; // return the status
}

//============================================================================
//!
//! @description
//! Finds the child state of the given @c parent, such that this child state
//! is an ancestor of the currently active state. The main purpose of this
//! function is to support **shallow history** transitions in state machines
//! derived from QHsm.
//!
//! @param[in] parent pointer to the state-handler function
//!
//! @returns
//! the child of a given @c parent state, which is an ancestor of the
//! currently active state
//!
//! @note
//! this function is designed to be called during state transitions, so it
//! does not necessarily start in a stable state configuration.
//! However, the function establishes stable state configuration upon exit.
//!
//! @tr{RQP103}
//! @tr{RQP120H}
//!
QStateHandler QHsm::childState(QStateHandler const parent) noexcept {
    QStateHandler child = m_state.fun; // start with the current state
    bool isFound = false; // start with the child not found

    // establish stable state configuration
    m_temp.fun = m_state.fun;
    QState r;
    do {
        // is this the parent of the current child?
        if (m_temp.fun == parent) {
            isFound = true; // child is found
            r = Q_RET_IGNORED;  // cause breaking out of the loop
        }
        else {
            child = m_temp.fun;
            r = QEP_TRIG_(m_temp.fun, QEP_EMPTY_SIG_);
        }
    } while (r != Q_RET_IGNORED); // QHsm::top() state not reached
    m_temp.fun = m_state.fun; // establish stable state configuration

    //! @post the child must be confirmed
    Q_ENSURE_ID(810, isFound);
#ifdef Q_NASSERT
    // avoid compiler warning about unused variable
    static_cast<void>(isFound);
#endif

    return child; // return the child
}

} // namespace QP

