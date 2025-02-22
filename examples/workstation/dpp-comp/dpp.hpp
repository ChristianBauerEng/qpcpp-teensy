//.$file${.::dpp.hpp} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
//
// Model: dpp-comp.qm
// File:  ${.::dpp.hpp}
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
//.$endhead${.::dpp.hpp} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#ifndef DPP_HPP
#define DPP_HPP

namespace DPP {

enum DPPSignals {
    TIMEOUT_SIG = QP::Q_USER_SIG, // time event timeout
    EAT_SIG,        // published by Table to let a philosopher eat
    DONE_SIG,       // published by Philosopher when done eating
    PAUSE_SIG,      // published by BSP to pause the application
    SERVE_SIG,      // published by BSP to serve re-start serving forks
    TEST_SIG,       // published by BSP to test the application
    MAX_PUB_SIG,    // the last published signal

    HUNGRY_SIG,     // posted direclty to Table from hungry Philo
    MAX_SIG         // the last signal
};

enum {
    N_PHILO = 5 // number of Philos
};

} // namespace DPP

//.$declare${Cont::TableEvt} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace DPP {

//.${Cont::TableEvt} .........................................................
class TableEvt : public QP::QEvt {
public:
    std::uint8_t philoNum;
};

} // namespace DPP
//.$enddecl${Cont::TableEvt} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//.$declare${Cont::AO_Table} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace DPP {

extern QP::QActive * const AO_Table;

} // namespace DPP
//.$enddecl${Cont::AO_Table} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

//.$declare${Comp::CompTimeEvt} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace DPP {

//.${Comp::CompTimeEvt} ......................................................
class CompTimeEvt : public QP::QTimeEvt {
public:
    std::uint16_t compNum;

public:
    CompTimeEvt(
        QP::QActive * act,
        std::uint16_t num,
        enum_t const sig,
        std::uint_fast8_t const tickRate);
};

} // namespace DPP
//.$enddecl${Comp::CompTimeEvt} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//.$declare${Comp::SM_Philo[N_PHILO]} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace DPP {

extern QP::QHsm * const SM_Philo[N_PHILO];

} // namespace DPP
//.$enddecl${Comp::SM_Philo[N_PHILO]} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

#endif // DPP_HPP