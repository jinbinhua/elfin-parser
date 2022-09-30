#ifndef _DWARFPP_SIG_HH__
#define _DWARFPP_SIG_HH__

#include <unistd.h>
#include <iostream>
#include <signal.h>

void siabrt_handler(int signal_num);

void sisegv_handler(int signal_num);

#endif