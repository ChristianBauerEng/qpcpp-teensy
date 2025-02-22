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
//! @brief QS software tracing services

#define QP_IMPL           // this is QP implementation
#include "qs_port.hpp"    // QS port
#include "qs_pkg.hpp"     // QS package-scope internal interface
#include "qstamp.hpp"     // QP time-stamp
#include "qassert.h"      // QP assertions

// unnamed namespace for local definitions with internal linkage
namespace {

Q_DEFINE_THIS_MODULE("qs")

} // unnamed namespace

namespace QP {

//============================================================================
QS QS::priv_; // QS private data

//============================================================================
//! @description
//! This function should be called from QP::QS::onStartup() to provide QS with
//! the data buffer. The first argument @a sto[] is the address of the memory
//! block, and the second argument @a stoSize is the size of this block
//! in bytes. Currently the size of the QS buffer cannot exceed 64KB.
//!
//! @note QS can work with quite small data buffers, but you will start losing
//! data if the buffer is too small for the bursts of tracing activity.
//! The right size of the buffer depends on the data production rate and
//! the data output rate. QS offers flexible filtering to reduce the data
//! production rate.
//!
//! @note If the data output rate cannot keep up with the production rate,
//! QS will start overwriting the older data with newer data. This is
//! consistent with the "last-is-best" QS policy. The record sequence counters
//! and check sums on each record allow the QSPY host uitiliy to easily detect
//! any data loss.
//!
void QS::initBuf(std::uint8_t * const sto,
                 std::uint_fast16_t const stoSize) noexcept
{
    // the provided buffer must be at least 8 bytes long
    Q_REQUIRE_ID(100, stoSize > 8U);

    // This function initializes all the internal QS variables, so that the
    // tracing can start correctly even if the startup code fails to clear
    // any uninitialized data (as is required by the C Standard).
    //
    glbFilter_(-static_cast<enum_t>(QS_ALL_RECORDS));// all global filters OFF
    locFilter_(static_cast<enum_t>(QS_ALL_IDS));     // all local filters ON
    priv_.locFilter_AP = nullptr; // deprecated "AP-filter"

    priv_.buf      = sto;
    priv_.end      = static_cast<QSCtr>(stoSize);
    priv_.head     = 0U;
    priv_.tail     = 0U;
    priv_.used     = 0U;
    priv_.seq      = 0U;
    priv_.chksum   = 0U;
    priv_.critNest = 0U;

    // produce an empty record to "flush" the QS trace buffer
    beginRec_(QS_REC_NUM_(QS_EMPTY));
    endRec_();

    // produce the Target info QS record
    QS_target_info_(0xFFU);

    // wait with flushing after successfull initialization (see QS_INIT())
}

//============================================================================
//! @description
//! This function sets up the QS filter to enable the record type @a filter.
//! The argument #QS_ALL_RECORDS specifies to filter-in all records.
//! This function should be called indirectly through the macro
//! QS_GLB_FILTER()
//!
//! @param[in] filter  the QS record-d or group to enable in the filter,
//!                 if positive or disable, if negative. The record-id
//!                 numbers must be in the range -127..127.
//! @note
//! Filtering based on the record-type is only the first layer of filtering.
//! The second layer is based on the object-type. Both filter layers must
//! be enabled for the QS record to be inserted in the QS buffer.
//!
//! @sa QP::QS::locFilter_()
//!
void QS::glbFilter_(std::int_fast16_t const filter) noexcept {
    bool const isRemove = (filter < 0);
    std::uint16_t const rec = isRemove
                  ? static_cast<std::uint16_t>(-filter)
                  : static_cast<std::uint16_t>(filter);
    switch (rec) {
        case QS_ALL_RECORDS: {
            std::uint8_t const tmp = (isRemove ? 0x00U : 0xFFU);
            std::uint_fast8_t i;
            // set all global filters (partially unrolled loop)
            for (i = 0U; i < Q_DIM(priv_.glbFilter); i += 4U) {
                priv_.glbFilter[i     ] = tmp;
                priv_.glbFilter[i + 1U] = tmp;
                priv_.glbFilter[i + 2U] = tmp;
                priv_.glbFilter[i + 3U] = tmp;
            }
            if (isRemove) {
                // leave the "not maskable" filters enabled,
                // see qs.h, Miscellaneous QS records (not maskable)
                //
                priv_.glbFilter[0] = 0x01U;
                priv_.glbFilter[7] = 0xFCU;
                priv_.glbFilter[8] = 0x7FU;
            }
            else {
                // never turn the last 3 records on (0x7D, 0x7E, 0x7F)
                priv_.glbFilter[15] = 0x1FU;
            }
            break;
        }
        case QS_SM_RECORDS:
            if (isRemove) {
                priv_.glbFilter[0] &=
                    static_cast<std::uint8_t>(~0xFEU & 0xFFU);
                priv_.glbFilter[1] &=
                    static_cast<std::uint8_t>(~0x03U & 0xFFU);
                priv_.glbFilter[6] &=
                    static_cast<std::uint8_t>(~0x80U & 0xFFU);
                priv_.glbFilter[7] &=
                    static_cast<std::uint8_t>(~0x03U & 0xFFU);
            }
            else {
                priv_.glbFilter[0] |= 0xFEU;
                priv_.glbFilter[1] |= 0x03U;
                priv_.glbFilter[6] |= 0x80U;
                priv_.glbFilter[7] |= 0x03U;
            }
            break;
        case QS_AO_RECORDS:
            if (isRemove) {
                priv_.glbFilter[1] &=
                    static_cast<std::uint8_t>(~0xFCU & 0xFFU);
                priv_.glbFilter[2] &=
                    static_cast<std::uint8_t>(~0x07U & 0xFFU);
                priv_.glbFilter[5] &=
                    static_cast<std::uint8_t>(~0x20U & 0xFFU);
            }
            else {
                priv_.glbFilter[1] |= 0xFCU;
                priv_.glbFilter[2] |= 0x07U;
                priv_.glbFilter[5] |= 0x20U;
            }
            break;
        case QS_EQ_RECORDS:
            if (isRemove) {
                priv_.glbFilter[2] &=
                    static_cast<std::uint8_t>(~0x78U & 0xFFU);
                priv_.glbFilter[5] &=
                    static_cast<std::uint8_t>(~0x40U & 0xFFU);
            }
            else {
                priv_.glbFilter[2] |= 0x78U;
                priv_.glbFilter[5] |= 0x40U;
            }
            break;
        case QS_MP_RECORDS:
            if (isRemove) {
                priv_.glbFilter[3] &=
                    static_cast<std::uint8_t>(~0x03U & 0xFFU);
                priv_.glbFilter[5] &=
                    static_cast<std::uint8_t>(~0x80U & 0xFFU);
            }
            else {
                priv_.glbFilter[3] |= 0x03U;
                priv_.glbFilter[5] |= 0x80U;
            }
            break;
        case QS_QF_RECORDS:
            if (isRemove) {
                priv_.glbFilter[2] &=
                    static_cast<std::uint8_t>(~0x80U & 0xFFU);
                priv_.glbFilter[3] &=
                    static_cast<std::uint8_t>(~0xFCU & 0xFFU);
                priv_.glbFilter[4] &=
                    static_cast<std::uint8_t>(~0xC0U & 0xFFU);
                priv_.glbFilter[5] &=
                    static_cast<std::uint8_t>(~0x1FU & 0xFFU);
            }
            else {
                priv_.glbFilter[2] |= 0x80U;
                priv_.glbFilter[3] |= 0xFCU;
                priv_.glbFilter[4] |= 0xC0U;
                priv_.glbFilter[5] |= 0x1FU;
            }
            break;
        case QS_TE_RECORDS:
            if (isRemove) {
                priv_.glbFilter[4] &=
                    static_cast<std::uint8_t>(~0x3FU & 0xFFU);
            }
            else {
                priv_.glbFilter[4] |= 0x3FU;
            }
            break;
        case QS_SC_RECORDS:
            if (isRemove) {
                priv_.glbFilter[6] &=
                    static_cast<std::uint8_t>(~0x7FU & 0xFFU);
            }
            else {
               priv_.glbFilter[6] |= 0x7FU;
            }
            break;
        case QS_U0_RECORDS:
            if (isRemove) {
                priv_.glbFilter[12] &=
                    static_cast<std::uint8_t>(~0xF0U & 0xFFU);
                priv_.glbFilter[13] &=
                    static_cast<std::uint8_t>(~0x01U & 0xFFU);
            }
            else {
                priv_.glbFilter[12] |= 0xF0U;
                priv_.glbFilter[13] |= 0x01U;
            }
            break;
        case QS_U1_RECORDS:
            if (isRemove) {
                priv_.glbFilter[13] &=
                    static_cast<std::uint8_t>(~0x3EU & 0xFFU);
            }
            else {
                priv_.glbFilter[13] |= 0x3EU;
            }
            break;
        case QS_U2_RECORDS:
            if (isRemove) {
                priv_.glbFilter[13] &=
                    static_cast<std::uint8_t>(~0xC0U & 0xFFU);
                priv_.glbFilter[14] &=
                    static_cast<std::uint8_t>(~0x07U & 0xFFU);
            }
            else {
                priv_.glbFilter[13] |= 0xC0U;
                priv_.glbFilter[14] |= 0x07U;
            }
            break;
        case QS_U3_RECORDS:
            if (isRemove) {
                priv_.glbFilter[14] &=
                    static_cast<std::uint8_t>(~0xF8U & 0xFFU);
            }
            else {
                priv_.glbFilter[14] |= 0xF8U;
            }
            break;
        case QS_U4_RECORDS:
            if (isRemove) {
                priv_.glbFilter[15] &= 0x1FU;
            }
            else {
                priv_.glbFilter[15] |= 0x1FU;
            }
            break;
        case QS_UA_RECORDS:
            if (isRemove) {
                priv_.glbFilter[12] &=
                    static_cast<std::uint8_t>(~0xF0U & 0xFFU);
                priv_.glbFilter[13] = 0U;
                priv_.glbFilter[14] = 0U;
                priv_.glbFilter[15] &=
                    static_cast<std::uint8_t>(~0x1FU & 0xFFU);
            }
            else {
                priv_.glbFilter[12] |= 0xF0U;
                priv_.glbFilter[13] |= 0xFFU;
                priv_.glbFilter[14] |= 0xFFU;
                priv_.glbFilter[15] |= 0x1FU;
            }
            break;
        default:
            // QS rec number can't exceed 0x7D, so no need for escaping
            Q_ASSERT_ID(210, rec < 0x7DU);

            if (isRemove) {
                priv_.glbFilter[rec >> 3U]
                    &= static_cast<std::uint8_t>(~(1U << (rec & 7U)) & 0xFFU);
            }
            else {
                priv_.glbFilter[rec >> 3U]
                    |= static_cast<std::uint8_t>(1U << (rec & 7U));
                // never turn the last 3 records on (0x7D, 0x7E, 0x7F)
                priv_.glbFilter[15] &= 0x1FU;
            }
            break;
    }
}

//============================================================================
//! @description
//! This function sets up the local QS filter to enable or disable the
//! given QS object-id or a group of object-ids @a filter.
//! This function should be called indirectly through the macro
//! QS_LOC_FILTER()
//!
//! @param[in] filter  the QS object-id or group to enable in the filter,
//!                 if positive or disable, if negative. The qs_id numbers
//!                 must be in the range 1..127.
//! @note
//! Filtering based on the object-id (local filter) is the second layer of
//! filtering. The first layer is based on the QS record-type (gloabl filter).
//! Both filter layers must be enabled for the QS record to be inserted into
//! the QS buffer.
//!
//! @sa QP::QS::glbFilter_()
//!
void QS::locFilter_(std::int_fast16_t const filter) noexcept {
    bool const isRemove = (filter < 0);
    std::uint16_t const qs_id = isRemove
                  ? static_cast<std::uint16_t>(-filter)
                  : static_cast<std::uint16_t>(filter);
    std::uint8_t const tmp = (isRemove ? 0x00U : 0xFFU);
    std::uint_fast8_t i;
    switch (qs_id) {
        case QS_ALL_IDS:
            // set all global filters (partially unrolled loop)
            for (i = 0U; i < Q_DIM(priv_.locFilter); i += 4U) {
                priv_.locFilter[i     ] = tmp;
                priv_.locFilter[i + 1U] = tmp;
                priv_.locFilter[i + 2U] = tmp;
                priv_.locFilter[i + 3U] = tmp;
            }
            break;
        case QS_AO_IDS:
            for (i = 0U; i < 8U; i += 4U) {
                priv_.locFilter[i     ] = tmp;
                priv_.locFilter[i + 1U] = tmp;
                priv_.locFilter[i + 2U] = tmp;
                priv_.locFilter[i + 3U] = tmp;
            }
            break;
        case QS_EP_IDS:
            i = 8U;
            priv_.locFilter[i     ] = tmp;
            priv_.locFilter[i + 1U] = tmp;
            break;
        case QS_AP_IDS:
            i = 12U;
            priv_.locFilter[i     ] = tmp;
            priv_.locFilter[i + 1U] = tmp;
            priv_.locFilter[i + 2U] = tmp;
            priv_.locFilter[i + 3U] = tmp;
            break;
        default:
            if (qs_id < 0x7FU) {
                if (isRemove) {
                    priv_.locFilter[qs_id >> 3U] &=
                        static_cast<std::uint8_t>(
                            ~(1U << (qs_id & 7U)) & 0xFFU);
                }
                else {
                    priv_.locFilter[qs_id >> 3U]
                        |= (1U << (qs_id & 7U));
                }
            }
            else {
                Q_ERROR_ID(310); // incorrect qs_id
            }
            break;
    }
    priv_.locFilter[0] |= 0x01U; // leave QS_ID == 0 always on
}

//============================================================================
//! @description
//! This function must be called at the beginning of each QS record.
//! This function should be called indirectly through the macro QS_BEGIN_ID(),
//! or QS_BEGIN_NOCRIT(), depending if it's called in a normal code or from
//! a critical section.
//!
void QS::beginRec_(std::uint_fast8_t const rec) noexcept {
    std::uint8_t const b = priv_.seq + 1U;
    std::uint8_t chksum_ = 0U; // reset the checksum
    std::uint8_t * const buf_   = priv_.buf; // put in a temporary (register)
    QSCtr head_      = priv_.head;  // put in a temporary (register)
    QSCtr const end_ = priv_.end;   // put in a temporary (register)

    priv_.seq = b; // store the incremented sequence num
    priv_.used = (priv_.used + 2U); // 2 bytes about to be added

    QS_INSERT_ESC_BYTE_(b)

    chksum_ += static_cast<std::uint8_t>(rec);
    QS_INSERT_BYTE_(static_cast<std::uint8_t>(rec)) // no need for escaping

    priv_.head   = head_;   // save the head
    priv_.chksum = chksum_; // save the checksum
}

//============================================================================
//! @description
//! This function must be called at the end of each QS record.
//! This function should be called indirectly through the macro QS_END(),
//! or QS_END_NOCRIT(), depending if it's called in a normal code or from
//! a critical section.
//!
void QS::endRec_(void) noexcept {
    std::uint8_t * const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr head_ = priv_.head;
    QSCtr const end_ = priv_.end;
    std::uint8_t b = priv_.chksum;
    b ^= 0xFFU; // invert the bits in the checksum

    priv_.used = (priv_.used + 2U); // 2 bytes about to be added

    if ((b != QS_FRAME) && (b != QS_ESC)) {
        QS_INSERT_BYTE_(b)
    }
    else {
        QS_INSERT_BYTE_(QS_ESC)
        QS_INSERT_BYTE_(b ^ QS_ESC_XOR)
        priv_.used = (priv_.used + 1U); // account for the ESC byte
    }

    QS_INSERT_BYTE_(QS_FRAME) // do not escape this QS_FRAME

    priv_.head = head_; // save the head
    if (priv_.used > end_) { // overrun over the old data?
        priv_.used = end_;   // the whole buffer is used
        priv_.tail = head_;  // shift the tail to the old data
    }
}

//============================================================================
void QS_target_info_(std::uint8_t const isReset) noexcept {
    static constexpr std::uint8_t ZERO = static_cast<std::uint8_t>('0');
    static std::uint8_t const * const TIME =
        reinterpret_cast<std::uint8_t const *>(&BUILD_TIME[0]);
    static std::uint8_t const * const DATE =
        reinterpret_cast<std::uint8_t const *>(&BUILD_DATE[0]);

    QS::beginRec_(static_cast<std::uint_fast8_t>(QS_TARGET_INFO));
        QS::u8_raw_(isReset);

        static union {
            std::uint16_t u16;
            std::uint8_t  u8[2];
        } endian_test;
        endian_test.u16 = 0x0102U;
        // big endian ? add the 0x8000U flag
        QS_U16_PRE_(((endian_test.u8[0] == 0x01U)
                    ? (0x8000U | QP_VERSION)
                    : QP_VERSION)); // target endianness + version number

        // send the object sizes...
        QS::u8_raw_(Q_SIGNAL_SIZE
                    | static_cast<std::uint8_t>(QF_EVENT_SIZ_SIZE << 4U));

#ifdef QF_EQUEUE_CTR_SIZE
        QS::u8_raw_(QF_EQUEUE_CTR_SIZE
                    | static_cast<std::uint8_t>(QF_TIMEEVT_CTR_SIZE << 4U));
#else
        QS::u8_raw_(static_cast<std::uint8_t>(QF_TIMEEVT_CTR_SIZE << 4U));
#endif // ifdef QF_EQUEUE_CTR_SIZE

#ifdef QF_MPOOL_CTR_SIZE
        QS::u8_raw_(QF_MPOOL_SIZ_SIZE
                    | static_cast<std::uint8_t>(QF_MPOOL_CTR_SIZE << 4U));
#else
        QS::u8_raw_(0U);
#endif // ifdef QF_MPOOL_CTR_SIZE

        QS::u8_raw_(QS_OBJ_PTR_SIZE | (QS_FUN_PTR_SIZE << 4U));
        QS::u8_raw_(QS_TIME_SIZE);

        // send the limits...
        QS::u8_raw_(QF_MAX_ACTIVE);
        QS::u8_raw_(QF_MAX_EPOOL | (QF_MAX_TICK_RATE << 4U));

        // send the build time in three bytes (sec, min, hour)...
        QS::u8_raw_((10U * (TIME[6] - ZERO)) + (TIME[7] - ZERO));
        QS::u8_raw_((10U * (TIME[3] - ZERO)) + (TIME[4] - ZERO));
        if (BUILD_TIME[0] == static_cast<std::uint8_t>(' ')) {
            QS::u8_raw_(TIME[1] - ZERO);
        }
        else {
            QS::u8_raw_((10U * (TIME[0] - ZERO)) + (TIME[1] - ZERO));
        }

        // send the build date in three bytes (day, month, year) ...
        if (BUILD_DATE[4] == static_cast<std::uint8_t>(' ')) {
            QS::u8_raw_(DATE[5] - ZERO);
        }
        else {
            QS::u8_raw_((10U * (DATE[4] - ZERO)) + (DATE[5] - ZERO));
        }
        // convert the 3-letter month to a number 1-12 ...
        std::uint8_t b;
        switch (DATE[0] + DATE[1] + DATE[2]) {
            case 'J' + 'a' +'n':
                b = 1U;
                break;
            case 'F' + 'e' + 'b':
                b = 2U;
                break;
            case 'M' + 'a' +'r':
                b = 3U;
                break;
            case 'A' + 'p' + 'r':
                b = 4U;
                break;
            case 'M' + 'a' + 'y':
                b = 5U;
                break;
            case 'J' + 'u' + 'n':
                b = 6U;
                break;
            case 'J' + 'u' + 'l':
                b = 7U;
                break;
            case 'A' + 'u' + 'g':
                b = 8U;
                break;
            case 'S' + 'e' + 'p':
                b = 9U;
                break;
            case 'O' + 'c' + 't':
                b = 10U;
                break;
            case 'N' + 'o' + 'v':
                b = 11U;
                break;
            case 'D' + 'e' + 'c':
                b = 12U;
                break;
            default:
                b = 0U;
                break;
        }
        QS::u8_raw_(b); // store the month
        QS::u8_raw_((10U * (DATE[9] - ZERO)) + (DATE[10] - ZERO));
    QS::endRec_();
}

//============================================================================
//! @description
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::u8_fmt_(std::uint8_t const format, std::uint8_t const d) noexcept {
    std::uint8_t chksum_ = priv_.chksum;  // put in a temporary (register)
    std::uint8_t *const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;    // put in a temporary (register)
    QSCtr const end_= priv_.end;     // put in a temporary (register)

    priv_.used = (priv_.used + 2U); // 2 bytes about to be added

    QS_INSERT_ESC_BYTE_(format)
    QS_INSERT_ESC_BYTE_(d)

    priv_.head   = head_;   // save the head
    priv_.chksum = chksum_; // save the checksum
}

//============================================================================
//! @description
//! This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::u16_fmt_(std::uint8_t format, std::uint16_t d) noexcept {
    std::uint8_t chksum_ = priv_.chksum; // put in a temporary (register)
    std::uint8_t * const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;   // put in a temporary (register)
    QSCtr const end_= priv_.end;    // put in a temporary (register)

    priv_.used = (priv_.used + 3U); // 3 bytes about to be added

    QS_INSERT_ESC_BYTE_(format)

    format = static_cast<std::uint8_t>(d);
    QS_INSERT_ESC_BYTE_(format)

    d >>= 8U;
    format = static_cast<std::uint8_t>(d);
    QS_INSERT_ESC_BYTE_(format)

    priv_.head   = head_;    // save the head
    priv_.chksum = chksum_;  // save the checksum
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::u32_fmt_(std::uint8_t format, std::uint32_t d) noexcept {
    std::uint8_t chksum_ = priv_.chksum;  // put in a temporary (register)
    std::uint8_t * const buf_= priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;    // put in a temporary (register)
    QSCtr const end_= priv_.end;     // put in a temporary (register)

    priv_.used = (priv_.used + 5U); // 5 bytes about to be added
    QS_INSERT_ESC_BYTE_(format) // insert the format byte

    for (std::uint_fast8_t i = 4U; i != 0U; --i) {
        format = static_cast<std::uint8_t>(d);
        QS_INSERT_ESC_BYTE_(format)
        d >>= 8U;
    }

    priv_.head   = head_;   // save the head
    priv_.chksum = chksum_; // save the checksum
}

//============================================================================
//! @note This function is only to be used through macro QS_USR_DICTIONARY()
//!
void QS::usr_dict_pre_(enum_t const rec,
                       char const * const name) noexcept
{
    QS_CRIT_STAT_
    QS_CRIT_E_();
    beginRec_(static_cast<std::uint_fast8_t>(QS_USR_DICT));
    QS_U8_PRE_(rec);
    QS_STR_PRE_(name);
    endRec_();
    QS_CRIT_X_();
    onFlush();
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::mem_fmt_(std::uint8_t const *blk, std::uint8_t size) noexcept {
    std::uint8_t b = static_cast<std::uint8_t>(MEM_T);
    std::uint8_t chksum_ = priv_.chksum + b;
    std::uint8_t * const buf_= priv_.buf; // put in a temporary (register)
    QSCtr head_     = priv_.head;         // put in a temporary (register)
    QSCtr const end_= priv_.end;          // put in a temporary (register)

    priv_.used = (priv_.used + size + 2U); // size+2 bytes to be added

    QS_INSERT_BYTE_(b)
    QS_INSERT_ESC_BYTE_(size)

    // output the 'size' number of bytes
    for (; size != 0U; --size) {
        b = *blk;
        QS_INSERT_ESC_BYTE_(b)
        ++blk;
    }

    priv_.head   = head_;    // save the head
    priv_.chksum = chksum_;  // save the checksum
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::str_fmt_(char const *s) noexcept {
    std::uint8_t b       = static_cast<std::uint8_t>(*s);
    std::uint8_t chksum_ = static_cast<std::uint8_t>(
                           priv_.chksum + static_cast<std::uint8_t>(STR_T));
    std::uint8_t * const buf_= priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;    // put in a temporary (register)
    QSCtr const end_= priv_.end;     // put in a temporary (register)
    QSCtr   used_   = priv_.used;    // put in a temporary (register)

    used_ += 2U; // the format byte and the terminating-0

    QS_INSERT_BYTE_(static_cast<std::uint8_t>(STR_T))
    while (b != 0U) {
        // ASCII characters don't need escaping
        chksum_ += b;  // update checksum
        QS_INSERT_BYTE_(b)
        ++s;
        b = static_cast<std::uint8_t>(*s);
        ++used_;
    }
    QS_INSERT_BYTE_(0U) // zero-terminate the string

    priv_.head   = head_;   // save the head
    priv_.chksum = chksum_; // save the checksum
    priv_.used   = used_;   // save # of used buffer space
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::u8_raw_(std::uint8_t const d) noexcept {
    std::uint8_t chksum_ = priv_.chksum;   // put in a temporary (register)
    std::uint8_t * const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;     // put in a temporary (register)
    QSCtr const end_= priv_.end;      // put in a temporary (register)

    priv_.used = (priv_.used + 1U);  // 1 byte about to be added
    QS_INSERT_ESC_BYTE_(d)

    priv_.head   = head_;   // save the head
    priv_.chksum = chksum_; // save the checksum
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::u8u8_raw_(std::uint8_t const d1, std::uint8_t const d2) noexcept {
    std::uint8_t chksum_ = priv_.chksum;   // put in a temporary (register)
    std::uint8_t * const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;     // put in a temporary (register)
    QSCtr const end_= priv_.end;      // put in a temporary (register)

    priv_.used = (priv_.used + 2U); // 2 bytes about to be added
    QS_INSERT_ESC_BYTE_(d1)
    QS_INSERT_ESC_BYTE_(d2)

    priv_.head   = head_;    // save the head
    priv_.chksum = chksum_;  // save the checksum
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::u16_raw_(std::uint16_t d) noexcept {
    std::uint8_t b = static_cast<std::uint8_t>(d);
    std::uint8_t chksum_ = priv_.chksum;   // put in a temporary (register)
    std::uint8_t * const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;     // put in a temporary (register)
    QSCtr const end_= priv_.end;      // put in a temporary (register)

    priv_.used = (priv_.used + 2U); // 2 bytes about to be added

    QS_INSERT_ESC_BYTE_(b)

    d >>= 8U;
    b = static_cast<std::uint8_t>(d);
    QS_INSERT_ESC_BYTE_(b)

    priv_.head   = head_;    // save the head
    priv_.chksum = chksum_;  // save the checksum
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::u32_raw_(std::uint32_t d) noexcept {
    std::uint8_t chksum_ = priv_.chksum;   // put in a temporary (register)
    std::uint8_t * const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;     // put in a temporary (register)
    QSCtr const end_= priv_.end;      // put in a temporary (register)

    priv_.used = (priv_.used + 4U); // 4 bytes about to be added
    for (std::uint_fast8_t i = 4U; i != 0U; --i) {
        std::uint8_t const b = static_cast<std::uint8_t>(d);
        QS_INSERT_ESC_BYTE_(b)
        d >>= 8U;
    }

    priv_.head   = head_;    // save the head
    priv_.chksum = chksum_;  // save the checksum
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::obj_raw_(void const * const obj) noexcept {
#if (QS_OBJ_PTR_SIZE == 1U)
    u8_raw_(reinterpret_cast<std::uint8_t>(obj));
#elif (QS_OBJ_PTR_SIZE == 2U)
    u16_raw_(reinterpret_cast<std::uint16_t>(obj));
#elif (QS_OBJ_PTR_SIZE == 4U)
    u32_raw_(reinterpret_cast<std::uint32_t>(obj));
#elif (QS_OBJ_PTR_SIZE == 8U)
    u64_raw_(reinterpret_cast<std::uint64_t>(obj));
#else
    u32_raw_(reinterpret_cast<std::uint32_t>(obj));
#endif
}

//============================================================================
//! @note This function is only to be used through macros, never in the
//! client code directly.
//!
void QS::str_raw_(char const *s) noexcept {
    std::uint8_t b = static_cast<std::uint8_t>(*s);
    std::uint8_t chksum_ = priv_.chksum;   // put in a temporary (register)
    std::uint8_t * const buf_ = priv_.buf; // put in a temporary (register)
    QSCtr   head_   = priv_.head;     // put in a temporary (register)
    QSCtr const end_= priv_.end;      // put in a temporary (register)
    QSCtr   used_   = priv_.used;     // put in a temporary (register)

    while (b != 0U) {
        chksum_ += b;      // update checksum
        QS_INSERT_BYTE_(b) // ASCII characters don't need escaping
        ++s;
        b = static_cast<std::uint8_t>(*s);
        ++used_;
    }
    QS_INSERT_BYTE_(0U) // zero-terminate the string
    ++used_;

    priv_.head   = head_;    // save the head
    priv_.chksum = chksum_;  // save the checksum
    priv_.used   = used_;    // save # of used buffer space
}

//============================================================================
//! @description
//! This function delivers one byte at a time from the QS data buffer.
//!
//! @returns the byte in the least-significant 8-bits of the 16-bit return
//! value if the byte is available. If no more data is available at the time,
//! the function returns QP::QS_EOD (End-Of-Data).
//!
//! @note QP::QS::getByte() is __not__ protected with a critical section.
//!
std::uint16_t QS::getByte(void) noexcept {
    std::uint16_t ret;
    if (priv_.used == 0U) {
        ret = QS_EOD; // set End-Of-Data
    }
    else {
        std::uint8_t const * const buf_ = priv_.buf; // put in a temporary
        QSCtr tail_ = priv_.tail;         // put in a temporary (register)

        // the byte to return
        ret = static_cast<std::uint16_t>(buf_[tail_]);

        ++tail_;  // advance the tail
        if (tail_ == priv_.end) {  // tail wrap around?
            tail_ = 0U;
        }
        priv_.tail = tail_;  // update the tail
        priv_.used = (priv_.used - 1U); // one less byte used
    }
    return ret;  // return the byte or EOD
}

//============================================================================
//! @description
//! This function delivers a contiguous block of data from the QS data buffer.
//! The function returns the pointer to the beginning of the block, and writes
//! the number of bytes in the block to the location pointed to by @a pNbytes.
//! The argument @a pNbytes is also used as input to provide the maximum size
//! of the data block that the caller can accept.
//!
//! @returns if data is available, the function returns pointer to the
//! contiguous block of data and sets the value pointed to by @p pNbytes
//! to the # available bytes. If data is available at the time the function is
//! called, the function returns NULL pointer and sets the value pointed to by
//! @p pNbytes to zero.
//!
//! @note
//! Only the NULL return from QP::QS::getBlock() indicates that the QS buffer
//! is empty at the time of the call. The non-NULL return often means that
//! the block is at the end of the buffer and you need to call
//! QP::QS::getBlock() again to obtain the rest of the data that
//! "wrapped around" to the beginning of the QS data buffer.
//!
//! @note QP::QS::getBlock() is __not__ protected with a critical section.
//!
std::uint8_t const *QS::getBlock(std::uint16_t * const pNbytes) noexcept {
    QSCtr const used_ = priv_.used; // put in a temporary (register)
    std::uint8_t *buf_;

    // any bytes used in the ring buffer?
    if (used_ == 0U) {
        *pNbytes = 0U;  // no bytes available right now
        buf_     = nullptr; // no bytes available right now
    }
    else {
        QSCtr tail_      = priv_.tail; // put in a temporary (register)
        QSCtr const end_ = priv_.end;  // put in a temporary (register)
        QSCtr n = static_cast<QSCtr>(end_ - tail_);
        if (n > used_) {
            n = used_;
        }
        if (n > static_cast<QSCtr>(*pNbytes)) {
            n = static_cast<QSCtr>(*pNbytes);
        }
        *pNbytes = static_cast<std::uint16_t>(n); // n-bytes available
        buf_ = priv_.buf;
        buf_ = &buf_[tail_]; // the bytes are at the tail

        priv_.used = (priv_.used - n);
        tail_     += n;
        if (tail_ == end_) {
            tail_ = 0U;
        }
        priv_.tail = tail_;
    }
    return buf_;
}

//============================================================================
//! @note This function is only to be used through macro QS_SIG_DICTIONARY()
//!
void QS::sig_dict_pre_(enum_t const sig, void const * const obj,
                       char const * const name) noexcept
{
    QS_CRIT_STAT_

    QS_CRIT_E_();
    beginRec_(static_cast<std::uint_fast8_t>(QS_SIG_DICT));
    QS_SIG_PRE_(sig);
    QS_OBJ_PRE_(obj);
    QS_STR_PRE_((*name == '&') ? &name[1] : name);
    endRec_();
    QS_CRIT_X_();
    onFlush();
}

//============================================================================
//! @note This function is only to be used through macro QS_OBJ_DICTIONARY()
//!
void QS::obj_dict_pre_(void const * const obj,
                       char const * const name) noexcept
{
    QS_CRIT_STAT_

    QS_CRIT_E_();
    beginRec_(static_cast<std::uint_fast8_t>(QS_OBJ_DICT));
    QS_OBJ_PRE_(obj);
    QS_STR_PRE_((*name == '&') ? &name[1] : name);
    endRec_();
    QS_CRIT_X_();
    onFlush();
}

//============================================================================
//! @note This function is only to be used through macro
//! QS_OBJ_ARR_DICTIONARY()
//!
void QS::obj_arr_dict_pre_(void const * const obj,
                           std::uint_fast16_t const idx,
                           char const * const name) noexcept
{
    Q_REQUIRE_ID(400, idx < 1000U);

    // format idx into a char buffer as "xxx\0"
    std::uint8_t idx_str[4];
    std::uint_fast16_t tmp = idx;
    std::uint8_t i;
    idx_str[3] = 0U; // zero-terminate
    idx_str[2] = static_cast<std::uint8_t>(
                    static_cast<std::uint8_t>('0') + (tmp % 10U));
    tmp /= 10U;
    idx_str[1] = static_cast<std::uint8_t>(
                    static_cast<std::uint8_t>('0') + (tmp % 10U));
    if (idx_str[1] == static_cast<std::uint8_t>('0')) {
       i = 2U;
    }
    else {
       tmp /= 10U;
       idx_str[0] = static_cast<std::uint8_t>(
                        static_cast<std::uint8_t>('0') + (tmp % 10U));
        if (idx_str[0] == static_cast<std::uint8_t>('0')) {
           i = 1U;
        }
        else {
           i = 0U;
        }
    }

    QS_CRIT_STAT_
    std::uint8_t j = ((*name == '&') ? 1U : 0U);

    QS_CRIT_E_();
    beginRec_(static_cast<std::uint_fast8_t>(QS_OBJ_DICT));
    QS_OBJ_PRE_(obj);
    for (; name[j] != '\0'; ++j) {
        QS_U8_PRE_(name[j]);
        if (name[j] == '[') {
            ++j;
            break;
        }
    }
    for (; idx_str[i] != 0U; ++i) {
        QS_U8_PRE_(idx_str[i]);
    }
    // skip chars until ']'
    for (; name[j] != '\0'; ++j) {
        if (name[j] == ']') {
            break;
        }
    }
    for (; name[j] != '\0'; ++j) {
        QS_U8_PRE_(name[j]);
    }
    QS_U8_PRE_(0U); // zero-terminate
    endRec_();
    QS_CRIT_X_();
    onFlush();
}

//============================================================================
//! @note This function is only to be used through macro QS_FUN_DICTIONARY()
//!
void QS::fun_dict_pre_(void (* const fun)(void),
                       char const * const name) noexcept
{
    QS_CRIT_STAT_

    QS_CRIT_E_();
    beginRec_(static_cast<std::uint_fast8_t>(QS_FUN_DICT));
    QS_FUN_PRE_(fun);
    QS_STR_PRE_((*name == '&') ? &name[1] : name);
    endRec_();
    QS_CRIT_X_();
    onFlush();
}

//============================================================================
//! @note This function is only to be used through macro QS_ASSERTION()
//!
void QS::assertion_pre_(char const * const module, int_t const loc,
                        std::uint32_t const delay)
{
    QS_BEGIN_NOCRIT_PRE_(QP::QS_ASSERT_FAIL, 0U)
        QS_TIME_PRE_();
        QS_U16_PRE_(loc);
        QS_STR_PRE_((module != nullptr) ? module : "?");
    QS_END_NOCRIT_PRE_()
    QP::QS::onFlush();
    for (std::uint32_t volatile ctr = delay; ctr > 0U; ) {
        ctr = (ctr - 1U);
    }
    QP::QS::onCleanup();
}

//............................................................................
void QS::crit_entry_pre_(void) {
    QS_BEGIN_NOCRIT_PRE_(QP::QS_QF_CRIT_ENTRY, 0U)
        QS_TIME_PRE_();
        QS::priv_.critNest = (QS::priv_.critNest + 1U);
        QS_U8_PRE_(QS::priv_.critNest);
    QS_END_NOCRIT_PRE_()
}

//............................................................................
void QS::crit_exit_pre_(void) {
    QS_BEGIN_NOCRIT_PRE_(QP::QS_QF_CRIT_EXIT, 0U)
        QS_TIME_PRE_();
        QS_U8_PRE_(QS::priv_.critNest);
        QS::priv_.critNest = (QS::priv_.critNest - 1U);
    QS_END_NOCRIT_PRE_()
}

//............................................................................
void QS::isr_entry_pre_(std::uint8_t const isrnest,
                        std::uint8_t const prio)
{
    QS_BEGIN_NOCRIT_PRE_(QP::QS_QF_ISR_ENTRY, 0U)
        QS_TIME_PRE_();
        QS_U8_PRE_(isrnest);
        QS_U8_PRE_(prio);
    QS_END_NOCRIT_PRE_()
}

//............................................................................
void QS::isr_exit_pre_(std::uint8_t const isrnest,
                       std::uint8_t const prio)
{
    QS_BEGIN_NOCRIT_PRE_(QP::QS_QF_ISR_EXIT, 0U)
        QS_TIME_PRE_();
        QS_U8_PRE_(isrnest);
        QS_U8_PRE_(prio);
    QS_END_NOCRIT_PRE_()
}

} // namespace QP
