/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  acquire is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  fastjson is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fastjson. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <CAENVMElib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <stdexcept>
#include <unistd.h>
#include <ctime>
#include <cmath>
#include <csignal>
#include <H5Cpp.h>
#include "json.hh"

using namespace std;
using namespace H5;

class VMEBridge {
    protected: 
        const string error_codes[6] = {"Success","Bus Error","Comm Error","Generic Error","Invalid Param","Timeout Error"};  
    
    public:
        VMEBridge(int link, int board) {
            int res = CAENVME_Init(cvV1718,link,board,&handle);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: Could not open VME bridge!";
                throw runtime_error(err.str());
            }
        }
        
        virtual ~VMEBridge() {
            int res = CAENVME_End(handle);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: Could not close VME bridge!";
                throw runtime_error(err.str());
            }
        }

        inline void write32(uint32_t addr, uint32_t data) {
            //cout << "\twrite32@" << hex << addr << ':' << data << dec << endl;
            int res = CAENVME_WriteCycle(handle, addr, &data, cvA32_U_DATA, cvD32);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: write32 @ " << hex << addr << " : " << data;
                throw runtime_error(err.str());
            }
            usleep(10000);
        }        
        
        inline uint32_t read32(uint32_t addr) {
            uint32_t read;
            usleep(1);
            //cout << "\tread32@" << hex << addr << ':';
            int res = CAENVME_ReadCycle(handle, addr, &read, cvA32_U_DATA, cvD32);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: read32 @ " << hex << addr;
                throw runtime_error(err.str());
            }
            //cout << read << dec << endl;
            return read;
        }
        
        inline void write16(uint32_t addr, uint32_t data) {
            //cout << "\twrite16@" << hex << addr << ':' << data << dec << endl;
            int res = CAENVME_WriteCycle(handle, addr, &data, cvA32_U_DATA, cvD16);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: write32 @ " << hex << addr << " : " << data;
                throw runtime_error(err.str());
            }
            usleep(10000);
        }        
        
        inline uint32_t read16(uint32_t addr) {
            uint32_t read;
            //cout << "\tread16@" << hex << addr << ':';
            int res = CAENVME_ReadCycle(handle, addr, &read, cvA32_U_DATA, cvD16);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: read32 @ " << hex << addr;
                throw runtime_error(err.str());
            }
            //cout << read << dec << endl;
            return read;
        }
        
        inline uint32_t readBLT(uint32_t addr, void *buffer, uint32_t size) {
            uint32_t bytes;
            //cout << "\tBLT@" << hex << addr << " for " << dec << size << endl;
            int res = CAENVME_MBLTReadCycle(handle, addr, buffer, size, cvA32_U_MBLT, (int*)&bytes);
            if (res && (res != -1)) { //we ignore bus errors for BLT
                stringstream err;
                err << error_codes[-res] << " :: readBLT @ " << hex << addr;
                throw runtime_error(err.str());
            }
            return bytes;
        }
        
    protected:
        int handle;
};

typedef struct {

    //REG_CHANNEL_ENABLE_MASK
    uint32_t enabled; // 1 bit
    
    //REG_GLOBAL_TRIGGER_MASK
    uint32_t global_trigger; // 1 bit
    
    //REG_TRIGGER_OUT_MASK
    uint32_t trg_out; // 1 bit
    
    //REG_RECORD_LENGTH
    uint32_t record_length; // 16* bit
    
    //REG_DYNAMIC_RANGE
    uint32_t dynamic_range; // 1 bit
    
    //REG_NEV_AGGREGATE
    uint32_t ev_per_buffer; // 10 bit
    
    //REG_PRE_TRG
    uint32_t pre_trigger; // 9* bit
    
    //REG_LONG_GATE
    uint32_t long_gate; // 12 bit
    
    //REG_SHORT_GATE
    uint32_t short_gate; // 12 bit
    
    //REG_PRE_GATE
    uint32_t gate_offset; // 8 bit
    
    //REG_DPP_TRG_THRESHOLD
    uint32_t trg_threshold; // 12 bit
    
    //REG_BASELINE_THRESHOLD
    uint32_t fixed_baseline; // 12 bit
    
    //REG_SHAPED_TRIGGER_WIDTH
    uint32_t shaped_trigger_width; // 10 bit
    
    //REG_TRIGGER_HOLDOFF
    uint32_t trigger_holdoff; // 10* bit
    
    //REG_DPP_CTRL
    uint32_t charge_sensitivity; // 3 bit (see docs)
    uint32_t pulse_polarity; // 1 bit (0->positive, 1->negative)
    uint32_t trigger_config; // 2 bit (see docs)
    uint32_t baseline_mean; // 3 bit (fixed, 16, 64, 256, 1024)
    uint32_t self_trigger; // 1 bit (0->enabled, 1->disabled)
    
    //REG_DC_OFFSET
    uint32_t dc_offset; // 16 bit (-1V to 1V)

} V1730_chan_config;

typedef struct {

    //REG_CONFIG
    uint32_t dual_trace; // 1 bit
    uint32_t analog_probe; // 2 bit (see docs)
    uint32_t oscilloscope_mode; // 1 bit
    uint32_t digital_virt_probe_1; // 3 bit (see docs)
    uint32_t digital_virt_probe_2; // 3 bit (see docs)
    
    //REG_GLOBAL_TRIGGER_MASK
    uint32_t coincidence_window; // 3 bit
    uint32_t global_majority_level; // 3 bit
    uint32_t external_global_trigger; // 1 bit
    uint32_t software_global_trigger; // 1 bit
    
    //REG_TRIGGER_OUT_MASK
    uint32_t out_logic; // 2 bit (OR,AND,MAJORITY)
    uint32_t out_majority_level; // 3 bit
    uint32_t external_trg_out; // 1 bit
    uint32_t software_trg_out; // 1 bit
    
    //REG_BUFF_ORG
    uint32_t buff_org;
    
    //REG_READOUT_BLT_AGGREGATE_NUMBER
    uint16_t max_board_agg_blt;
    
} V1730_card_config;

class V1730Settings {
    friend class V1730;

    public:
    
        V1730Settings() {
            //These are "do nothing" defaults
            card.dual_trace = 0; // 1 bit
            card.analog_probe = 0; // 2 bit (see docs)
            card.oscilloscope_mode = 1; // 1 bit
            card.digital_virt_probe_1 = 0; // 3 bit (see docs)
            card.digital_virt_probe_2 = 0; // 3 bit (see docs)
            card.coincidence_window = 1; // 3 bit
            card.global_majority_level = 0; // 3 bit
            card.external_global_trigger = 0; // 1 bit
            card.software_global_trigger = 0; // 1 bit
            card.out_logic = 0; // 2 bit (OR,AND,MAJORITY)
            card.out_majority_level = 0; // 3 bit
            card.external_trg_out = 0; // 1 bit
            card.software_trg_out = 0; // 1 bit
            card.max_board_agg_blt = 5;
            
            for (uint32_t ch = 0; ch < 16; ch++) {
                chanDefaults(ch);
            }
            
        }
        
        V1730Settings(map<string,json::Value> &db) {
            
            json::Value &digitizer = db["DIGITIZER[]"];
            
            card.dual_trace = 0; // 1 bit
            card.analog_probe = 0; // 2 bit (see docs)
            card.oscilloscope_mode = 1; // 1 bit
            card.digital_virt_probe_1 = 0; // 3 bit (see docs)
            card.digital_virt_probe_2 = 0; // 3 bit (see docs)
            card.coincidence_window = 1; // 3 bit
            
            card.global_majority_level = digitizer["global_majority_level"].cast<int>(); // 3 bit
            card.external_global_trigger = digitizer["external_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
            card.software_global_trigger = digitizer["software_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
            card.out_logic = digitizer["trig_out_logic"].cast<int>(); // 2 bit (OR,AND,MAJORITY)
            card.out_majority_level = digitizer["trig_out_majority_level"].cast<int>(); // 3 bit
            card.external_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
            card.software_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
            card.max_board_agg_blt = digitizer["aggregates_per_transfer"].cast<int>();
            
            for (int ch = 0; ch < 16; ch++) {
                string chname = "CH["+to_string(ch)+"]";
                if (db.find(chname) == db.end()) {
                    chanDefaults(ch);
                } else {
                    json::Value &channel = db[chname];
            
                    chans[ch].dynamic_range = 0; // 1 bit
                    chans[ch].fixed_baseline = 0; // 12 bit
                    
                    chans[ch].enabled = channel["enabled"].cast<bool>() ? 1 : 0; //1 bit
                    chans[ch].global_trigger = channel["request_global_trigger"].cast<bool>() ? 1 : 0; // 1 bit
                    chans[ch].trg_out = channel["request_trig_out"].cast<bool>() ? 1 : 0; // 1 bit
                    chans[ch].record_length = channel["record_length"].cast<int>(); // 16* bit
                    chans[ch].dc_offset = round((channel["dc_offset"].cast<double>()+1.0)/2.0*pow(2.0,16.0)); // 16 bit (-1V to 1V)
                    chans[ch].baseline_mean = channel["baseline_average"].cast<int>(); // 3 bit (fixed,16,64,256,1024)
                    chans[ch].pulse_polarity = channel["pulse_polarity"].cast<int>(); // 1 bit (0->positive, 1->negative)
                    chans[ch].trg_threshold =  channel["trigger_threshold"].cast<int>();// 12 bit
                    chans[ch].self_trigger = channel["self_trigger"].cast<bool>() ? 1 : 0; // 1 bit (0->enabled, 1->disabled)
                    chans[ch].pre_trigger = channel["pre_trigger"].cast<int>(); // 9* bit
                    chans[ch].gate_offset = channel["gate_offset"].cast<int>(); // 8 bit
                    chans[ch].long_gate = channel["long_gate"].cast<int>(); // 12 bit
                    chans[ch].short_gate = channel["short_gate"].cast<int>(); // 12 bit
                    chans[ch].charge_sensitivity = channel["charge_sensitivity"].cast<int>(); // 3 bit (see docs)
                    chans[ch].shaped_trigger_width = channel["shaped_trigger_width"].cast<int>(); // 10 bit
                    chans[ch].trigger_holdoff = channel["trigger_holdoff"].cast<int>(); // 10* bit
                    chans[ch].trigger_config = channel["trigger_type"].cast<int>(); // 2 bit (see docs)
                    chans[ch].ev_per_buffer = channel["events_per_buffer"].cast<int>(); // 10 bit
                }
            }
        }
        
        virtual ~V1730Settings() {
        
        }
        
        void validate() {
            for (int ch = 0; ch < 16; ch++) {
                if (ch % 2 == 0) {
                    if (chans[ch].record_length > 65535) throw runtime_error("Number of samples exceeds 65535 (ch " + to_string(ch) + ")");
                } else {
                    if (chans[ch].record_length != chans[ch-1].record_length) throw runtime_error("Record length is not the same between pairs (ch " + to_string(ch) + ")"); 
                }
                if (chans[ch].ev_per_buffer < 2) throw runtime_error("Number of events per channel buffer must be at least 2 (ch " + to_string(ch) + ")");
                if (chans[ch].ev_per_buffer > 1023) throw runtime_error("Number of events per channel buffer exceeds 1023 (ch " + to_string(ch) + ")");
                if (chans[ch].pre_trigger > 2044) throw runtime_error("Pre trigger samples exceeds 2044 (ch " + to_string(ch) + ")");
                if (chans[ch].short_gate > 4095) throw runtime_error("Short gate samples exceeds 4095 (ch " + to_string(ch) + ")");
                if (chans[ch].long_gate > 4095) throw runtime_error("Long gate samples exceeds 4095 (ch " + to_string(ch) + ")");
                if (chans[ch].gate_offset > 255) throw runtime_error("Gate offset samples exceeds 255 (ch " + to_string(ch) + ")");
                if (chans[ch].pre_trigger < chans[ch].gate_offset + 19) throw runtime_error("Gate offset and pre_trigger relationship violated (ch " + to_string(ch) + ")");
                if (chans[ch].trg_threshold > 4095) throw runtime_error("Trigger threshold exceeds 4095 (ch " + to_string(ch) + ")");
                if (chans[ch].fixed_baseline > 4095) throw runtime_error("Fixed baseline exceeds 4095 (ch " + to_string(ch) + ")");
                if (chans[ch].shaped_trigger_width > 1023) throw runtime_error("Shaped trigger width exceeds 1023 (ch " + to_string(ch) + ")");
                if (chans[ch].trigger_holdoff > 4092) throw runtime_error("Trigger holdoff width exceeds 4092 (ch " + to_string(ch) + ")");
                if (chans[ch].dc_offset > 65535) throw runtime_error("DC Offset cannot exceed 65535 (ch " + to_string(ch) + ")");
            }
        }
        
        inline bool getEnabled(uint32_t ch) {
            return chans[ch].enabled;
        }
        
        inline uint32_t getRecordLength(uint32_t ch) {
            return chans[ch].record_length;
        }
        
        inline uint32_t getDCOffset(uint32_t ch) {
            return chans[ch].dc_offset;
        }
        
        inline uint32_t getPreSamples(uint32_t ch) {
            return chans[ch].pre_trigger;
        }
        
        inline uint32_t getThreshold(uint32_t ch) {
            return chans[ch].trg_threshold;
        }
    
    protected:
    
        V1730_card_config card;
        V1730_chan_config chans[16];
        
        void chanDefaults(uint32_t ch) {
            chans[ch].enabled = 0; //1 bit
            chans[ch].global_trigger = 0; // 1 bit
            chans[ch].trg_out = 0; // 1 bit
            chans[ch].record_length = 200; // 16* bit
            chans[ch].dynamic_range = 0; // 1 bit
            chans[ch].ev_per_buffer = 50; // 10 bit
            chans[ch].pre_trigger = 30; // 9* bit
            chans[ch].long_gate = 20; // 12 bit
            chans[ch].short_gate = 10; // 12 bit
            chans[ch].gate_offset = 5; // 8 bit
            chans[ch].trg_threshold = 100; // 12 bit
            chans[ch].fixed_baseline = 0; // 12 bit
            chans[ch].shaped_trigger_width = 10; // 10 bit
            chans[ch].trigger_holdoff = 30; // 10* bit
            chans[ch].charge_sensitivity = 000; // 3 bit (see docs)
            chans[ch].pulse_polarity = 1; // 1 bit (0->positive, 1->negative)
            chans[ch].trigger_config = 0; // 2 bit (see docs)
            chans[ch].baseline_mean = 3; // 3 bit (fixed,16,64,256,1024)
            chans[ch].self_trigger = 1; // 1 bit (0->enabled, 1->disabled)
            chans[ch].dc_offset = 0x8000; // 16 bit (-1V to 1V)
        }
};

class V1730 {

    //system wide
    #define REG_CONFIG 0x8000
    #define REG_CONFIG_SET 0x8004
    #define REG_CONFIG_CLEAR 0x8008
    #define REG_BUFF_ORG 0x800C
    #define REG_FRONT_PANEL_CONTROL 0x811C
    #define REG_DUMMY 0xEF20
    #define REG_SOFTWARE_RESET 0xEF24
    #define REG_SOFTWARE_CLEAR 0xEF28
    #define REG_BOARD_CONFIGURATION_RELOAD 0xEF34

    //per channel, or with 0x0n00
    #define REG_RECORD_LENGTH 0x1020
    #define REG_DYNAMIC_RANGE 0x1024
    #define REG_NEV_AGGREGATE 0x1034
    #define REG_PRE_TRG 0x1038
    #define REG_SHORT_GATE 0x1054
    #define REG_LONG_GATE 0x1058
    #define REG_PRE_GATE 0x105C
    #define REG_DPP_TRG_THRESHOLD 0x1060
    #define REG_BASELINE_THRESHOLD 0x1064
    #define REG_SHAPED_TRIGGER_WIDTH 0x1070
    #define REG_TRIGGER_HOLDOFF 0x1074
    #define REG_DPP_CTRL 0x1080
    #define REG_TRIGGER_CTRL 0x1084
    #define REG_DC_OFFSET 0x1098
    #define REG_CHANNEL_TEMP 0x10A8

    //acquisition 
    #define REG_ACQUISITION_CONTROL 0x8100
    #define REG_ACQUISITION_STATUS 0x8104
    #define REG_SOFTWARE_TRIGGER 0x8108
    #define REG_GLOBAL_TRIGGER_MASK 0x810C
    #define REG_TRIGGER_OUT_MASK 0x8110
    #define REG_CHANNEL_ENABLE_MASK 0x8120

    //readout
    #define REG_EVENT_SIZE 0x814C
    #define REG_READOUT_CONTROL 0xEF00
    #define REG_READOUT_STATUS 0xEF04
    #define REG_VME_ADDRESS_RELOCATION 0xEF10
    #define REG_READOUT_BLT_AGGREGATE_NUMBER 0xEF1C
    
    public:
        V1730(VMEBridge &_bridge, uint32_t _baseaddr, bool busErrReadout = true) : bridge(_bridge), baseaddr(_baseaddr), berr(busErrReadout) {
        
        }
        
        virtual ~V1730() {
            //Fully reset the board just in case
            write32(REG_BOARD_CONFIGURATION_RELOAD,0);
        }
        
        bool program(V1730Settings &settings) {
            try {
                settings.validate();
            } catch (runtime_error &e) {
                cout << "Could not program V1730: " << e.what() << endl;
                return false;
            }
        
            //used to build bit fields
            uint32_t data;
            
            //Fully reset the board just in case
            write32(REG_BOARD_CONFIGURATION_RELOAD,0);
            
            //Set TTL logic levels, ignore LVDS and debug settings
            write32(REG_FRONT_PANEL_CONTROL,1);
    
            data = (1 << 4) 
                 | (1 << 8) 
                 | (settings.card.dual_trace << 11) 
                 | (settings.card.analog_probe << 12) 
                 | (settings.card.oscilloscope_mode << 16) 
                 | (1 << 17) 
                 | (1 << 18)
                 | (1 << 19)
                 | (settings.card.digital_virt_probe_1 << 23)
                 | (settings.card.digital_virt_probe_2 << 26);
            write32(REG_CONFIG,data);
    
            //build masks while configuring channels
            uint32_t channel_enable_mask = 0;
            uint32_t global_trigger_mask = (settings.card.coincidence_window << 20)
                                         | (settings.card.global_majority_level << 24) 
                                         | (settings.card.external_global_trigger << 30) 
                                         | (settings.card.software_global_trigger << 31);
            uint32_t trigger_out_mask = (settings.card.out_logic << 8) 
                                      | (settings.card.out_majority_level << 10)
                                      | (settings.card.external_trg_out << 30) 
                                      | (settings.card.software_trg_out << 31);
            
            //keep track of the size of the buffers for each memory location
            uint32_t buffer_sizes[8] = { 0, 0, 0, 0, 0, 0, 0, 0}; //in locations
    
            //keep track of how to config the local triggers
            uint32_t local_logic[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    
            for (int ch = 0; ch < 16; ch++) {
                channel_enable_mask |= (settings.chans[ch].enabled << ch);
                global_trigger_mask |= (settings.chans[ch].global_trigger << (ch/2));
                trigger_out_mask |= (settings.chans[ch].trg_out << (ch/2));
                
                if (ch % 2 == 0) { //memory shared between pairs
                
                    if (settings.chans[ch].global_trigger) {
                        if (settings.chans[ch+1].global_trigger) {
                            local_logic[ch/2] = 3;
                        } else {
                            local_logic[ch/2] = 1;
                        }
                    } else {
                        if (settings.chans[ch+1].global_trigger) {
                            local_logic[ch/2] = 2;
                        } else {
                            local_logic[ch/2] = 0;
                        }
                    }
                    
                    data = settings.chans[ch].record_length%8 ?  settings.chans[ch].record_length/8+1 : settings.chans[ch].record_length/8;
                    write32(REG_RECORD_LENGTH|(ch<<8),data);
                    settings.chans[ch].record_length = read32(REG_RECORD_LENGTH|(ch<<8))*8;
                } else {
                    settings.chans[ch].record_length = settings.chans[ch-1].record_length;
                    settings.chans[ch].ev_per_buffer = settings.chans[ch-1].ev_per_buffer;
                }
                
                if (settings.chans[ch].enabled) {
                    buffer_sizes[ch/2] = (2 + settings.chans[ch].record_length/8)*settings.chans[ch].ev_per_buffer;
                }
                
                write32(REG_NEV_AGGREGATE|(ch<<8),settings.chans[ch].ev_per_buffer);
                write32(REG_PRE_TRG|(ch<<8),settings.chans[ch].pre_trigger/4);
                write32(REG_SHORT_GATE|(ch<<8),settings.chans[ch].short_gate);
                write32(REG_LONG_GATE|(ch<<8),settings.chans[ch].long_gate);
                write32(REG_PRE_GATE|(ch<<8),settings.chans[ch].gate_offset);
                write32(REG_DPP_TRG_THRESHOLD|(ch<<8),settings.chans[ch].trg_threshold);
                write32(REG_BASELINE_THRESHOLD|(ch<<8),settings.chans[ch].fixed_baseline);
                write32(REG_SHAPED_TRIGGER_WIDTH|(ch<<8),settings.chans[ch].shaped_trigger_width);
                write32(REG_TRIGGER_HOLDOFF|(ch<<8),settings.chans[ch].trigger_holdoff/4);
                data = (settings.chans[ch].charge_sensitivity)
                     | (settings.chans[ch].pulse_polarity << 16)
                     | (settings.chans[ch].trigger_config << 18)
                     | (settings.chans[ch].baseline_mean << 20)
                     | (settings.chans[ch].self_trigger << 24);
                write32(REG_DPP_CTRL|(ch<<8),data);
                data = local_logic[ch/2] | (1<<2);
                write32(REG_TRIGGER_CTRL|(ch<<8),data);
                write32(REG_DC_OFFSET|(ch<<8),settings.chans[ch].dc_offset);
                
            }
            
            write32(REG_CHANNEL_ENABLE_MASK,channel_enable_mask);
            write32(REG_GLOBAL_TRIGGER_MASK,global_trigger_mask);
            write32(REG_TRIGGER_OUT_MASK,trigger_out_mask);
            
            uint32_t largest_buffer = 0;
            for (int i = 0; i < 8; i++) if (largest_buffer < buffer_sizes[i]) largest_buffer = buffer_sizes[i];
            const uint32_t total_locations = 640000/8; 
            const uint32_t num_buffers = total_locations/largest_buffer;
            uint32_t shifter = num_buffers;
            for (settings.card.buff_org = 0; shifter != 1; shifter = shifter >> 1, settings.card.buff_org++);
            if (1 << settings.card.buff_org < num_buffers) settings.card.buff_org++;
            if (settings.card.buff_org > 0xA) settings.card.buff_org = 0xA;
            if (settings.card.buff_org < 0x2) settings.card.buff_org = 0x2;
            cout << "Largest buffer: " << largest_buffer << " loc\nDesired buffers: " << num_buffers << "\nProgrammed buffers: " << (1<<settings.card.buff_org) << endl;
            write32(REG_BUFF_ORG,settings.card.buff_org);
            
            //Set max board aggregates to transver per readout
            write16(REG_READOUT_BLT_AGGREGATE_NUMBER,settings.card.max_board_agg_blt);
            
            //Enable VME BLT readout
            write16(REG_READOUT_CONTROL,1<<4);
            
            return true;
        }
        
        void startAcquisition() {
            write32(REG_ACQUISITION_CONTROL,1<<2);
        }
        
        void stopAcquisition() {
            write32(REG_ACQUISITION_CONTROL,0);
        }
        
        bool acquisitionRunning() {
            return read32(REG_ACQUISITION_STATUS) & (1 << 2);
        }
        
        bool readoutReady() {
            return read32(REG_ACQUISITION_STATUS) & (1 << 3);
        }
        
        bool checkTemps(vector<uint32_t> &temps, uint32_t danger) {
            temps.resize(16);
            bool over = false;
            for (int ch = 0; ch < 16; ch++) {
                temps[ch] = read32(REG_CHANNEL_TEMP|(ch<<8));
                if (temps[ch] >= danger) over = true;
            }
            return over;
        }
        
        inline size_t readoutBLT(char *buffer, size_t buffer_size) {
            return berr ? readoutBLT_berr(buffer, buffer_size) : readoutBLT_evtsz(buffer,buffer_size);
        }
        
        inline void write16(uint32_t reg, uint32_t data) {
            bridge.write16(baseaddr|reg,data);
        }
        
        inline uint32_t read16(uint32_t reg) {
            return bridge.read16(baseaddr|reg);
        }
        
        inline void write32(uint32_t reg, uint32_t data) {
            bridge.write32(baseaddr|reg,data);
        }
        
        inline uint32_t read32(uint32_t reg) {
            return bridge.read32(baseaddr|reg);
        }
        
        inline uint32_t readBLT(uint32_t addr, void *buffer, uint32_t size) {
            return bridge.readBLT(baseaddr|addr,buffer,size);
        }
        
    protected:
        
        bool berr;
        VMEBridge &bridge;
        uint32_t baseaddr;
        
        //for bus error read
        size_t readoutBLT_berr(char *buffer, size_t buffer_size) {
            size_t offset = 0, size = 0;
            while (offset < buffer_size && (size = readBLT(0x0000, buffer+offset, 4090))) {
                offset += size;
            }
            return offset;
        }
        
        //for event size read
        size_t readoutBLT_evtsz(char *buffer, size_t buffer_size) {
            size_t offset = 0, total = 0;
            while (readoutReady()) {
                uint32_t next = read32(REG_EVENT_SIZE);
                total += 4*next;
                if (offset+total > buffer_size) throw runtime_error("Readout buffer too small!");
                size_t lastoff = offset;
                while (offset < total) {
                    size_t remaining = total-offset, read;
                    if (remaining > 4090) {
                        read = readBLT(0x0000, buffer+offset, 4090);
                    } else {
                        remaining = 8*(remaining%8 ? remaining/8+1 : remaining/8); // needs to be multiples of 8 (64bit)
                        read = readBLT(0x0000, buffer+offset, remaining);
                    }
                    offset += read;
                    if (!read) {
                        cout << "\tfailed event size " << offset-lastoff << " / " << next*4 << endl;
                        break;
                    }
                }
            }
            return total;
        }
        
};

class EventReadout {

    public:
    
        // for data acquisition
        EventReadout(size_t nGrabs, size_t nRepeat, string outfile, V1730Settings &_settings) : settings(_settings) {
            init(nGrabs,nRepeat,outfile);
        }
        
        // for debugging / trigger rate
        EventReadout(V1730Settings &_settings) : settings(_settings) {
            init(0,0,"none");
        }
        
        virtual ~EventReadout() {
            delete [] buffer;
            for (size_t i = 0; i < grabs.size(); i++) {
                delete [] grabs[i];
                delete [] baselines[i];
                delete [] qshorts[i];
                delete [] qlongs[i];
                delete [] times[i];
            }
        }
        
        bool readout(V1730 &dgtz) {
        
            struct timespec this_time;
            clock_gettime(CLOCK_MONOTONIC,&this_time);
            time_int = (this_time.tv_sec-last_time.tv_sec)+(this_time.tv_nsec-last_time.tv_nsec)*1e-9;
            last_time = this_time;
        
            readout_size = dgtz.readoutBLT(buffer,buffer_size);
            readout_counter++;
            
            uint32_t *next = (uint32_t*)buffer, *start = (uint32_t*)buffer;
            while (((next = decode_board_agg(next)) - start + 1)*4 < readout_size) {
                
            }
            
            if (nGrabs) {
                bool done = true;
                for (int idx = 0; idx < grabbed.size(); idx++) {
                    if (grabbed[idx] < nGrabs) done = false;
                }
                if (done) { 
                    writeout();
                    if (cycle+1 == nRepeat || nRepeat == 0) return false;
                    for (int idx = 0; idx < grabbed.size(); idx++) {
                        grabbed[idx] = 0;
                    }
                    cycle++;
                }
            }
            
            return true;
        }
        
        void stats() {
            if (nRepeat > 0) cout << "acquisition cycle: " << cycle+1 << " / " << nRepeat << endl; 
            cout << "readout " << readout_counter << " : " << readout_size << " bytes / " << readout_size/1024.0/time_int << " KiB/s" << endl;
            for (size_t i = 0; i < idx2chan.size(); i++) {
                cout << "\tch" << idx2chan[i] << "\tev: " << lastgrabbed[i] << " / " << lastgrabbed[i]/time_int << " Hz" << endl;
            }
        }

    protected: 
        
        V1730Settings &settings;
        
        struct timespec last_time;
        double time_int;
        
        size_t cycle;    
        size_t readout_counter;
        size_t chanagg_counter;
        size_t boardagg_counter;
        
        size_t buffer_size, readout_size;
        char *buffer;
            
        size_t nGrabs, nRepeat;
        string outfile;    
        
        map<uint32_t,uint32_t> chan2idx,idx2chan;
        vector<size_t> nsamples;
        vector<size_t> grabbed,lastgrabbed;
        vector<uint16_t*> grabs, baselines, qshorts, qlongs;
        vector<uint32_t*> times;
        
        void init(size_t nGrabs, size_t nRepeat, string outfile) {
            this->nGrabs = nGrabs;
            this->nRepeat = nRepeat;
            this->outfile = outfile;
            
            clock_gettime(CLOCK_MONOTONIC,&last_time);
            
            cycle = readout_counter = chanagg_counter = boardagg_counter = 0;
            buffer_size = 100*1024*1024;
            buffer = new char[buffer_size];
            
            for (size_t ch = 0; ch < 16; ch++) {
                if (settings.getEnabled(ch)) {
                    chan2idx[ch] = nsamples.size();
                    idx2chan[nsamples.size()] = ch;
                    nsamples.push_back(settings.getRecordLength(ch));
                    grabbed.push_back(0);
                    lastgrabbed.push_back(0);
                    if (nGrabs > 0) {
                        grabs.push_back(new uint16_t[nGrabs*nsamples.back()]);
                        baselines.push_back(new uint16_t[nGrabs]);
                        qshorts.push_back(new uint16_t[nGrabs]);
                        qlongs.push_back(new uint16_t[nGrabs]);
                        times.push_back(new uint32_t[nGrabs]);
                    }
                }
            }
            
        }

        uint32_t* decode_chan_agg(uint32_t *chanagg, uint32_t group) {
            const bool format_flag = chanagg[0] & 0x80000000;
            if (!format_flag) throw runtime_error("Channel format not found");
            
            chanagg_counter++;
            
            const uint32_t size = chanagg[0] & 0x7FFF;
            const uint32_t format = chanagg[1];
            const uint32_t samples = (format & 0xFFF)*8;
            
            /*
            //Metadata
            const bool dualtrace_enable = format & (1<<31);
            const bool charge_enable =format & (1<<30);
            const bool time_enable = format & (1<<29);
            const bool baseline_enable = format & (1<<28);
            const bool waveform_enable = format & (1<<27);
            const uint32_t extras = (format >> 24) & 0x7;
            const uint32_t analog_probe = (format >> 22) & 0x3;
            const uint32_t digital_probe_2 = (format >> 19) & 0x7;
            const uint32_t digital_probe_1 = (format >> 16) & 0x7;
            */
            
            const uint32_t idx0 = chan2idx.count(group * 2 + 0) ? chan2idx[group * 2 + 0] : 999;
            const uint32_t idx1 = chan2idx.count(group * 2 + 1) ? chan2idx[group * 2 + 1] : 999;
            
            if (idx0 != 999) lastgrabbed[idx0] = grabbed[idx0];
            if (idx1 != 999) lastgrabbed[idx1] = grabbed[idx1];
                
            for (uint32_t *event = chanagg+2; event-chanagg+1 < size; event += samples/2+3) {
                
                const bool oddch = event[0] & 0x80000000;
                const uint32_t idx = oddch ? idx1 : idx0;
                const uint32_t len = nsamples[idx];
                
                if (idx == 999) throw runtime_error("Received data for disabled channel (" + to_string(group*2+oddch?1:0) + ")");
                
                if (len != samples) throw runtime_error("Number of samples received does not match expected (" + to_string(idx2chan[idx]) + ")");
                
                if (nGrabs && grabbed[idx] < nGrabs) {
                    
                    const size_t ev = grabbed[idx]++;
                    uint16_t *data = grabs[idx];
                    
                    for (uint32_t *word = event+1, sample = 0; sample < len; word++, sample+=2) {
                        data[ev*len+sample+0] = *word & 0x3FFF;
                        //uint8_t dp10 = (*word >> 14) & 0x1;
                        //uint8_t dp20 = (*word >> 15) & 0x1;
                        data[ev*len+sample+1] = (*word >> 16) & 0x3FFF;
                        //uint8_t dp11 = (*word >> 30) & 0x1;
                        //uint8_t dp21 = (*word >> 31) & 0x1;
                    }
                    
                    baselines[idx][ev] = event[1+samples/2+0] & 0xFFFF;
                    //uint16_t extratime = (event[1+samples/2+0] >> 16) & 0xFFFF;
                    qshorts[idx][ev] = event[1+samples/2+1] & 0x7FFF;
                    qlongs[idx][ev] = (event[1+samples/2+1] >> 16) & 0xFFFF;
                    times[idx][ev] = event[0] & 0x7FFFFFFF;
                    
                } else {
                    grabbed[idx]++;
                }
            
            }
                
            if (idx0 != 999) lastgrabbed[idx0] = grabbed[idx0] - lastgrabbed[idx0];
            if (idx1 != 999) lastgrabbed[idx1] = grabbed[idx1] - lastgrabbed[idx1];
            
            return chanagg + size;
        }

        uint32_t* decode_board_agg(uint32_t *boardagg) {
            if (boardagg[0] == 0xFFFFFFFF) {
                boardagg++; //sometimes padded
                cout << "padded\n";
            }
            if ((boardagg[0] & 0xF0000000) != 0xA0000000) 
                throw runtime_error("Board aggregate missing tag");
            
            boardagg_counter++;    
            
            uint32_t size = boardagg[0] & 0x0FFFFFFF;
            
            const uint32_t board = (boardagg[1] >> 28) & 0xF;
            const bool fail = boardagg[1] & (1 << 26);
            const uint32_t pattern = (boardagg[1] >> 8) & 0x7FFF;
            const uint32_t mask = boardagg[1] & 0xFF;
            
            const uint32_t count = boardagg[2] & 0x7FFFFF;
            const uint32_t timetag = boardagg[3];
            
            uint32_t *chans = boardagg+4;
            
            for (uint32_t gr = 0; gr < 8; gr++) {
                if (mask & (1 << gr)) {
                    chans = decode_chan_agg(chans,gr);
                }
            } 
            
            return boardagg+size;
        }
        
        void writeout() {
            Exception::dontPrint();
        
            string fname = outfile;
            if (nRepeat > 0) {
                fname += "." + to_string(cycle);
            }
            fname += ".h5"; 
            
            cout << "Saving data to " << fname << endl;
            
            H5File file(fname, H5F_ACC_TRUNC);
            
            for (size_t i = 0; i < nsamples.size(); i++) {
                cout << "Dumping channel " << idx2chan[i] << "... ";
            
                string groupname = "/ch" + to_string(idx2chan[i]);
                Group group = file.createGroup(groupname);
                
                cout << "Attributes, ";
                
                DataSpace scalar(0,NULL);
                
                double dval;
                uint32_t ival;
                
                Attribute bits = group.createAttribute("bits",PredType::NATIVE_UINT32,scalar);
                ival = 14;
                bits.write(PredType::NATIVE_INT32,&ival);
                
                Attribute ns_sample = group.createAttribute("ns_sample",PredType::NATIVE_DOUBLE,scalar);
                dval = 2.0;
                ns_sample.write(PredType::NATIVE_DOUBLE,&dval);
                
                Attribute offset = group.createAttribute("offset",PredType::NATIVE_UINT32,scalar);
                ival = settings.getDCOffset(idx2chan[i]);
                offset.write(PredType::NATIVE_UINT32,&ival);
                
                Attribute samples = group.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
                ival = settings.getRecordLength(idx2chan[i]);
                offset.write(PredType::NATIVE_UINT32,&ival);
                
                Attribute presamples = group.createAttribute("presamples",PredType::NATIVE_UINT32,scalar);
                ival = settings.getPreSamples(idx2chan[i]);
                offset.write(PredType::NATIVE_UINT32,&ival);
                
                Attribute threshold = group.createAttribute("threshold",PredType::NATIVE_UINT32,scalar);
                ival = settings.getThreshold(idx2chan[i]);
                offset.write(PredType::NATIVE_UINT32,&ival);
                
                hsize_t dimensions[2];
                dimensions[0] = nGrabs;
                dimensions[1] = nsamples[i];
                
                DataSpace samplespace(2, dimensions);
                DataSpace metaspace(1, dimensions);
                
                cout << "Samples, ";
                DataSet samples_ds = file.createDataSet(groupname+"/samples", PredType::NATIVE_UINT16, samplespace);
                samples_ds.write(grabs[i], PredType::NATIVE_UINT16);
                
                cout << "Baselines, ";
                DataSet baselines_ds = file.createDataSet(groupname+"/baselines", PredType::NATIVE_UINT16, metaspace);
                baselines_ds.write(baselines[i], PredType::NATIVE_UINT16);
                
                cout << "QShorts, ";
                DataSet qshorts_ds = file.createDataSet(groupname+"/qshorts", PredType::NATIVE_UINT16, metaspace);
                qshorts_ds.write(qshorts[i], PredType::NATIVE_UINT16);
                
                cout << "QLongs, ";
                DataSet qlongs_ds = file.createDataSet(groupname+"/qlongs", PredType::NATIVE_UINT16, metaspace);
                qlongs_ds.write(qlongs[i], PredType::NATIVE_UINT16);

                cout << "Times";
                DataSet times_ds = file.createDataSet(groupname+"/times", PredType::NATIVE_UINT32, metaspace);
                times_ds.write(times[i], PredType::NATIVE_UINT32);
                
                cout << endl;
            }
        }

};

map<string,json::Value> readDB(string file) {

    ifstream dbfile;
    dbfile.open(file);
    if (!dbfile.is_open()) throw runtime_error("Could not open " + file);
    
    json::Reader reader(dbfile);
    dbfile.close();
    map<string,json::Value> db;
    
    json::Value next;
    while (reader.getValue(next)) {
        if (next.getType() != json::TOBJECT) throw runtime_error("DB contains non-object values");
        string name = next["name"].cast<string>();
        if (next.isMember("index")) {
            db[name+"["+next["index"].cast<string>()+"]"] = next;
        } else {
            db[name+"[]"] = next;
        }
    }
    
    return db;
}

bool running;

void int_handler(int x) {
    running = false;
}

int main(int argc, char **argv) {

    if (argc != 2) {
        cout << "./v1730_dpppsd config.json" << endl;
        return -1;
    }

    running = true;
    signal(SIGINT,int_handler);
    
    cout << "Reading configuration..." << endl;
    
    map<string,json::Value> db = readDB(argv[1]);
    json::Value run = db["RUN[]"];
    
    const int ngrabs = run["events"].cast<int>();
    const string outfile = run["outfile"].cast<string>();
    const int linknum = run["link_num"].cast<int>();
    const int baseaddr = run["base_address"].cast<int>();
    const int checktemps = run["check_temps"].cast<bool>();
    int nrepeat;
    if (run.isMember("repeat_times")) {
        nrepeat = run["repeat_times"].cast<int>();
    } else {
        nrepeat = 0;
    }

    cout << "Opening VME link..." << endl;
    
    VMEBridge bridge(linknum,0);
    V1730 dgtz(bridge,baseaddr);
   
    cout << "Programming Digitizer..." << endl;
    
    V1730Settings settings(db);
    
    if (!dgtz.program(settings)) return -1;
    
    cout << "Starting acquisition..." << endl;
    
    EventReadout acq(ngrabs,nrepeat,outfile,settings);
    vector<uint32_t> temps;
    dgtz.startAcquisition();
    
    for (size_t counter = 0; running; counter++) {
    
        while (!dgtz.readoutReady() && running) {
            if (!dgtz.acquisitionRunning()) break;
        }
        if (!dgtz.acquisitionRunning()) {
            cout << "Digitizer aborted acquisition!" << endl;
            break;
        }
        
        if (!acq.readout(dgtz)) running = false;
        acq.stats();
        
        if (checktemps && counter % 100 == 0) {
            bool overtemp = dgtz.checkTemps(temps,60);
            cout << "temps: [ " << temps[0];
            for (int ch = 1; ch < 16; ch++) cout << ", " << temps[ch];
            cout << " ]" << endl;
            if (overtemp) {
                cout << "Overtemp! Aborting readout." << endl;
                break;
            }
        }
        
    }
    
    cout << "Stopping acquisition..." << endl;
    
    dgtz.stopAcquisition();
    
    return 0;

}
