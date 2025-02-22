//.$file${.::worker.cpp} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
//
// Model: start-stop.qm
// File:  ${.::worker.cpp}
//
// This code has been generated by QM 5.1.3 <www.state-machine.com/qm/>.
// DO NOT EDIT THIS FILE MANUALLY. All your changes will be lost.
//
// This program is open source software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published
// by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
//.$endhead${.::worker.cpp} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#include "qpcpp.hpp"
#include "Worker.hpp"
#include "bsp.hpp"

//Q_DEFINE_THIS_FILE

//.$skip${QP_VERSION} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
//. Check for the minimum required QP version
#if (QP_VERSION < 690U) || (QP_VERSION != ((QP_RELEASE^4294967295U) % 0x3E8U))
#error qpcpp version 6.9.0 or higher required
#endif
//.$endskip${QP_VERSION} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//.$define${AOs::Worker} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
//.${AOs::Worker} ............................................................
Worker Worker::inst;
//.${AOs::Worker::Worker} ....................................................
Worker::Worker()
  : QActive(&initial),
    m_te(this, TIMEOUT_SIG, 0U)
{}

//.${AOs::Worker::~Worker} ...................................................
Worker::~Worker() {
    BSP::led2Off();
}

//.${AOs::Worker::SM} ........................................................
Q_STATE_DEF(Worker, initial) {
    //.${AOs::Worker::SM::initial}
    (void)e; // unused parameter

    QS_OBJ_DICTIONARY(&Worker::inst);
    QS_OBJ_DICTIONARY(&Worker::inst.m_te);

    QS_FUN_DICTIONARY(&Worker::active);
    QS_FUN_DICTIONARY(&Worker::off);
    QS_FUN_DICTIONARY(&Worker::on);
    QS_FUN_DICTIONARY(&Worker::final);

    return tran(&off);
}
//.${AOs::Worker::SM::active} ................................................
Q_STATE_DEF(Worker, active) {
    QP::QState status_;
    switch (e->sig) {
        //.${AOs::Worker::SM::active}
        case Q_ENTRY_SIG: {
            m_counter = 5U; // number of blinks
            m_te.armX(BSP::TICKS_PER_SEC / 5U,
                      BSP::TICKS_PER_SEC / 5U);
            status_ = Q_RET_HANDLED;
            break;
        }
        //.${AOs::Worker::SM::active}
        case Q_EXIT_SIG: {
            m_te.disarm();
            status_ = Q_RET_HANDLED;
            break;
        }
        default: {
            status_ = super(&top);
            break;
        }
    }
    return status_;
}
//.${AOs::Worker::SM::active::off} ...........................................
Q_STATE_DEF(Worker, off) {
    QP::QState status_;
    switch (e->sig) {
        //.${AOs::Worker::SM::active::off::TIMEOUT}
        case TIMEOUT_SIG: {
            status_ = tran(&on);
            break;
        }
        default: {
            status_ = super(&active);
            break;
        }
    }
    return status_;
}
//.${AOs::Worker::SM::active::on} ............................................
Q_STATE_DEF(Worker, on) {
    QP::QState status_;
    switch (e->sig) {
        //.${AOs::Worker::SM::active::on}
        case Q_ENTRY_SIG: {
            BSP::ledOn();
            status_ = Q_RET_HANDLED;
            break;
        }
        //.${AOs::Worker::SM::active::on}
        case Q_EXIT_SIG: {
            BSP::ledOff();
            status_ = Q_RET_HANDLED;
            break;
        }
        //.${AOs::Worker::SM::active::on::TIMEOUT}
        case TIMEOUT_SIG: {
            --m_counter;
            //.${AOs::Worker::SM::active::on::TIMEOUT::[m_counter==0U]}
            if (m_counter == 0U) {
                status_ = tran(&final);
            }
            //.${AOs::Worker::SM::active::on::TIMEOUT::[else]}
            else {
                status_ = tran(&off);
            }
            break;
        }
        default: {
            status_ = super(&active);
            break;
        }
    }
    return status_;
}
//.${AOs::Worker::SM::final} .................................................
Q_STATE_DEF(Worker, final) {
    QP::QState status_;
    switch (e->sig) {
        //.${AOs::Worker::SM::final}
        case Q_ENTRY_SIG: {
            BSP::led2On();
            static QEvt const doneEvt = { DONE_SIG, 0 };
            QF::PUBLISH(&doneEvt, this);
            stop(); // stop this active object
            status_ = Q_RET_HANDLED;
            break;
        }
        default: {
            status_ = super(&top);
            break;
        }
    }
    return status_;
}
//.$enddef${AOs::Worker} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
