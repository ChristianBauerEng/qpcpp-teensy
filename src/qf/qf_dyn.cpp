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
//! @brief QF/C++ dynamic event management

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

Q_DEFINE_THIS_MODULE("qf_dyn")

} // unnamed namespace

//============================================================================
namespace QP {

// Package-scope objects *****************************************************
QF_EPOOL_TYPE_ QF_pool_[QF_MAX_EPOOL]; // allocate the event pools
std::uint_fast8_t QF_maxPool_; // number of initialized event pools

//............................................................................
void QF::poolInit(void * const poolSto,
                  std::uint_fast32_t const poolSize,
                  std::uint_fast16_t const evtSize) noexcept
{
    //! @pre cannot exceed the number of available memory pools
    Q_REQUIRE_ID(200, QF_maxPool_ < QF_MAX_EPOOL);

    //! @pre please initialize event pools in ascending order of evtSize
    Q_REQUIRE_ID(201, (QF_maxPool_ == 0U)
        || (QF_EPOOL_EVENT_SIZE_(QF_pool_[QF_maxPool_ - 1U])
            < evtSize));

    QF_EPOOL_INIT_(QF_pool_[QF_maxPool_], poolSto, poolSize, evtSize);
    ++QF_maxPool_; // one more pool

#ifdef Q_SPY
    // generate the object-dictionary entry for the initialized pool
    char obj_name[9] = "EvtPool?";
    obj_name[7] = static_cast<char>(
        static_cast<std::int8_t>('0')
        + static_cast<std::int8_t>(QF_maxPool_));
    QS::obj_dict_pre_(&QF_pool_[QF_maxPool_ - 1U], &obj_name[0]);
#endif // Q_SPY
}

//============================================================================
QEvt *QF::newX_(std::uint_fast16_t const evtSize,
                std::uint_fast16_t const margin, enum_t const sig) noexcept
{
    std::uint_fast8_t idx;

    // find the pool id that fits the requested event size ...
    for (idx = 0U; idx < QF_maxPool_; ++idx) {
        if (evtSize <= QF_EPOOL_EVENT_SIZE_(QF_pool_[idx])) {
            break;
        }
    }
    // cannot run out of registered pools
    Q_ASSERT_ID(310, idx < QF_maxPool_);

    // get e -- platform-dependent
    QEvt *e;

#ifdef Q_SPY
    QF_EPOOL_GET_(QF_pool_[idx], e, ((margin != QF_NO_MARGIN) ? margin : 0U),
                  static_cast<std::uint_fast8_t>(QS_EP_ID) + idx + 1U);
#else
    QF_EPOOL_GET_(QF_pool_[idx], e, ((margin != QF_NO_MARGIN) ? margin : 0U),
                  0U);
#endif

    // was e allocated correctly?
    QS_CRIT_STAT_
    if (e != nullptr) {
        e->sig     = static_cast<QSignal>(sig); // set the signal
        e->poolId_ = static_cast<std::uint8_t>(idx + 1U); // store pool ID
        e->refCtr_ = 0U; // initialize the reference counter to 0

        QS_BEGIN_PRE_(QS_QF_NEW,
                      static_cast<std::uint_fast8_t>(QS_EP_ID)
                         + static_cast<std::uint_fast8_t>(e->poolId_))
            QS_TIME_PRE_();       // timestamp
            QS_EVS_PRE_(evtSize); // the size of the evt
            QS_SIG_PRE_(sig);     // the signal of the evt
        QS_END_PRE_()
    }
    else {
        // This assertion means that the event allocation failed,
        // and this failure cannot be tolerated. The most frequent
        // reason is an event leak in the application.
        Q_ASSERT_ID(320, margin != QF_NO_MARGIN);

        QS_BEGIN_PRE_(QS_QF_NEW_ATTEMPT,
                      static_cast<std::uint_fast8_t>(QS_EP_ID) + idx + 1U)
            QS_TIME_PRE_();       // timestamp
            QS_EVS_PRE_(evtSize); // the size of the evt
            QS_SIG_PRE_(sig);     // the signal of the evt
        QS_END_PRE_()
    }
    return e; // can't be NULL if we can't tolerate bad allocation
}

//============================================================================
void QF::gc(QEvt const * const e) noexcept {
    // is it a dynamic event?
    if (e->poolId_ != 0U) {
        QF_CRIT_STAT_
        QF_CRIT_E_();

        // isn't this the last reference?
        if (e->refCtr_ > 1U) {

            QS_BEGIN_NOCRIT_PRE_(QS_QF_GC_ATTEMPT,
                     static_cast<std::uint_fast8_t>(QS_EP_ID)
                         + static_cast<std::uint_fast8_t>(e->poolId_))
                QS_TIME_PRE_();        // timestamp
                QS_SIG_PRE_(e->sig);   // the signal of the event
                QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool Id & refCtr
            QS_END_NOCRIT_PRE_()

            QF_EVT_REF_CTR_DEC_(e); // decrement the ref counter

            QF_CRIT_X_();
        }
        // this is the last reference to this event, recycle it
        else {
            std::uint_fast8_t const idx =
                static_cast<std::uint_fast8_t>(e->poolId_) - 1U;

            QS_BEGIN_NOCRIT_PRE_(QS_QF_GC,
                     static_cast<std::uint_fast8_t>(QS_EP_ID)
                         + static_cast<std::uint_fast8_t>(e->poolId_))
                QS_TIME_PRE_();        // timestamp
                QS_SIG_PRE_(e->sig);   // the signal of the event
                QS_2U8_PRE_(e->poolId_, e->refCtr_);
            QS_END_NOCRIT_PRE_()

            QF_CRIT_X_();

            // pool ID must be in range
            Q_ASSERT_ID(410, idx < QF_maxPool_);

#ifdef Q_EVT_XTOR
            // explicitly exectute the destructor'
            // NOTE: casting 'const' away is legitimate,
            // because it's a pool event
            QF_EVT_CONST_CAST_(e)->~QEvt(); // xtor,
#endif

#ifdef Q_SPY
            // cast 'const' away, which is OK, because it's a pool event
            QF_EPOOL_PUT_(QF_pool_[idx], QF_EVT_CONST_CAST_(e),
                     static_cast<std::uint_fast8_t>(QS_EP_ID)
                         + static_cast<std::uint_fast8_t>(e->poolId_));
#else
            QF_EPOOL_PUT_(QF_pool_[idx], QF_EVT_CONST_CAST_(e), 0U);
#endif
        }
    }
}

//============================================================================
QEvt const *QF::newRef_(QEvt const * const e,
                        QEvt const * const evtRef) noexcept
{
    //! @pre the event must be dynamic and the provided event reference
    //! must not be already in use
    Q_REQUIRE_ID(500, (e->poolId_ != 0U)
                      && (evtRef == nullptr));

    QF_CRIT_STAT_
    QF_CRIT_E_();

    QF_EVT_REF_CTR_INC_(e); // increments the ref counter

    QS_BEGIN_NOCRIT_PRE_(QS_QF_NEW_REF,
                     static_cast<std::uint_fast8_t>(QS_EP_ID)
                         + static_cast<std::uint_fast8_t>(e->poolId_))
        QS_TIME_PRE_();      // timestamp
        QS_SIG_PRE_(e->sig); // the signal of the event
        QS_2U8_PRE_(e->poolId_, e->refCtr_); // pool Id & ref Count
    QS_END_NOCRIT_PRE_()

    QF_CRIT_X_();

    return e;
}

//============================================================================
void QF::deleteRef_(QEvt const * const evtRef) noexcept {
    QS_CRIT_STAT_

    QS_BEGIN_PRE_(QS_QF_DELETE_REF,
                     static_cast<std::uint_fast8_t>(QS_EP_ID)
                         + static_cast<std::uint_fast8_t>(evtRef->poolId_))
        QS_TIME_PRE_();           // timestamp
        QS_SIG_PRE_(evtRef->sig); // the signal of the event
        QS_2U8_PRE_(evtRef->poolId_, evtRef->refCtr_); // pool Id & ref Count
    QS_END_PRE_()

    gc(evtRef); // recycle the referenced event
}

//============================================================================
//! @description
//! Obtain the block size of any registered event pools
//!
std::uint_fast16_t QF::poolGetMaxBlockSize(void) noexcept {
    return QF_EPOOL_EVENT_SIZE_(QF_pool_[QF_maxPool_ - 1U]);
}

} // namespace QP
