#ifndef AQL_MAIN_H
#define AQL_MAIN_H



enum {                   // Stopp reasons for main loop
    NON_STOPP,           // init value, keep on running
    STOPP_FOR_REBOOT,    // rebooting only
    STOPP_FOR_OTA,       // stopp mainloop and start OTA
};

extern uint8_t stopp_reason;


#endif

