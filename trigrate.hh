/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  trigrate is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  trigrate is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fastjson. If not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef __ACQUIRE__HH
#define __ACQUIRE__HH

#include <array>
#include <vector>
#include <string>
#include <stdexcept>

#include <CAENDigitizer.h>

static const std::array<CAEN_DGTZ_RunSyncMode_t,5> json_sync_mode = {
    CAEN_DGTZ_RUN_SYNC_Disabled, 
    CAEN_DGTZ_RUN_SYNC_TrgOutTrgInDaisyChain, 
    CAEN_DGTZ_RUN_SYNC_TrgOutSinDaisyChain, 
    CAEN_DGTZ_RUN_SYNC_SinFanout, 
    CAEN_DGTZ_RUN_SYNC_GpioGpioDaisyChain
};

static const std::array<CAEN_DGTZ_IOLevel_t,2> json_io_level = {
    CAEN_DGTZ_IOLevel_NIM, 
    CAEN_DGTZ_IOLevel_TTL
};

static const std::array<CAEN_DGTZ_AcqMode_t,2> json_acq_mode = {
    CAEN_DGTZ_SW_CONTROLLED, 
    CAEN_DGTZ_S_IN_CONTROLLED
};

static const std::array<CAEN_DGTZ_TriggerMode_t,4> json_trig_mode = {
    CAEN_DGTZ_TRGMODE_DISABLED, 
    CAEN_DGTZ_TRGMODE_EXTOUT_ONLY, 
    CAEN_DGTZ_TRGMODE_ACQ_ONLY, 
    CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT
};

static const std::array<CAEN_DGTZ_DPP_AcqMode_t,3> json_dpp_acq_mode = {
    CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope, 
    CAEN_DGTZ_DPP_ACQ_MODE_List, 
    CAEN_DGTZ_DPP_ACQ_MODE_Mixed
};

static const std::array<CAEN_DGTZ_DPP_SaveParam_t,5> json_dpp_acq_param = {
    CAEN_DGTZ_DPP_SAVE_PARAM_EnergyOnly, 
    CAEN_DGTZ_DPP_SAVE_PARAM_TimeOnly, 
    CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime, 
    CAEN_DGTZ_DPP_SAVE_PARAM_ChargeAndTime, 
    CAEN_DGTZ_DPP_SAVE_PARAM_None
};

static const std::array<CAEN_DGTZ_DPP_TriggerMode_t,5> json_dpp_trig_mode = {
    CAEN_DGTZ_DPP_TriggerMode_Normal,
    CAEN_DGTZ_DPP_TriggerMode_Coincidence
};

static const std::array<CAEN_DGTZ_PulsePolarity_t,2> json_pulse_polarity = {
    CAEN_DGTZ_PulsePolarityPositive,
    CAEN_DGTZ_PulsePolarityNegative
};

typedef struct {
    CAEN_DGTZ_EnaDis_t state;
    uint8_t level;
    uint32_t status_id;
    uint16_t event_number;
    CAEN_DGTZ_IRQMode_t mode;
} InterruptConfig;

typedef struct {
    CAEN_DGTZ_TriggerMode_t sw;
    CAEN_DGTZ_TriggerMode_t ext;
    CAEN_DGTZ_DPP_TriggerMode_t dpp;
} TriggerConfig;

typedef struct {
    bool enabled;
    uint32_t samples, presamples;
    uint32_t offset;
    
    int threshold;
    int chargesens;
    int baseline, coincidence;
    int shortgate, longgate, pregate;
    bool selftrig;
    
    uint32_t eventsperagg;
    CAEN_DGTZ_TriggerMode_t trigmode;
    CAEN_DGTZ_PulsePolarity_t pulsepol;
} ChannelConfig;

typedef struct {
    CAEN_DGTZ_BoardInfo_t info;    
    
    bool config_inter;
    InterruptConfig inter;
    TriggerConfig trig;
    
    int trigholdoff;
    
    std::vector<ChannelConfig> chans;
    
    CAEN_DGTZ_RunSyncMode_t sync;
    CAEN_DGTZ_IOLevel_t iolevel;
    
    CAEN_DGTZ_AcqMode_t acqmode;
    CAEN_DGTZ_DPP_AcqMode_t dppacqmode;
    CAEN_DGTZ_DPP_SaveParam_t dppacqparam;
    
    uint32_t aggperblt;
} Settings;

inline std::string CAENERR(int code) {
    switch (code) {
        case -1: return "Communication error";
        case -2: return "Unspecified error";
        case -3: return "Invalid parameter";
        case -4: return "Invalid Link Type";
        case -5: return "Invalid device handler";
        case -6: return "Maximum number of devices exceeded";
        case -7: return "Operation not allowed on this type of board";
        case -8: return "The interrupt level is not allowed";
        case -9: return "The event number is bad";
        case -10: return "Unable to read the registry";
        case -11: return "Unable to write into the registry";
        case -13: return "The Channel is busy";
        case -14: return "The channel number is invalid";
        case -15: return "Invalid FPIO Mode";
        case -16: return "Wrong acquisition mode";
        case -17: return "This function is not allowed for this module";
        case -18: return "Communication Timeout";
        case -19: return "The buffer is invalid";
        case -20: return "The event is not found";
        case -21: return "The event is invalid";
        case -22: return "Out of memory";
        case -23: return "Unable to calibrate the board";
        case -24: return "Unable to open the digitizer";
        case -25: return "The Digitizer is already open";
        case -26: return "The Digitizer is not ready to operate";
        case -27: return "The Digitizer has not the IRQ configured";
        case -28: return "The digitizer flash memory is corrupted";
        case -29: return "The digitizer DPP firmware is not supported in this lib version";
        case -30: return "Invalid Firmware License";
        case -31: return "The digitizer is found in a corrupted status";
        case -32: return "The given trace is not supported by the digitizer";
        case -33: return "The given probe is not supported for the given digitizer's trace";
        case -99: return "The function is not yet implemented";
        default: return "ERROR UNDEFINED";
    }
}

#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)

//To wrap CAEN library calls and throw nice exceptions on error
#define SAFE(call) { \
    const int err = call; \
    if (err) throw std::runtime_error("CAEN Error (" + CAENERR(err) + ") at " __FILE__ ":" S__LINE__ " " #call);  \
}

#endif
