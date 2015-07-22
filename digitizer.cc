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
 
#include "digitizer.hh"

#include <iostream>
#include <fstream>

using namespace std;

map<string,json::Value> ReadDB(string file) {

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

void InitSettings(int handle, Settings &settings) {
    SAFE(CAEN_DGTZ_GetInfo(handle, &settings.info));
    settings.chans.resize(settings.info.Channels);
    
    cout << "Opened digitizer: " << settings.info.ModelName << endl;
    cout << settings.info.Channels << " channels @ " << settings.info.ADC_NBits << " bits" << endl;
    cout << "Digitizer family: " << settings.info.FamilyCode << endl;
    cout << "ROC FPGA Release: " << settings.info.ROC_FirmwareRel << endl;
    cout << "AMC FPGA Release: " << settings.info.AMC_FirmwareRel << endl;
}

void SettingsFromDB(map<string,json::Value> &db, Settings &settings) {
    json::Value &run = db["RUN[]"];
    json::Value &digitizer = db["DIGITIZER[]"];

    settings.config_inter = false; // do not set up any interrupts
    
    settings.dppacqmode = json_dpp_acq_mode[digitizer["dpp_acq_mode"].cast<int>()];
    settings.dppacqparam = json_dpp_acq_param[digitizer["dpp_acq_param"].cast<int>()];
    
    settings.acqmode = json_acq_mode[digitizer["acq_mode"].cast<int>()];
    settings.sync = json_sync_mode[digitizer["sync_mode"].cast<int>()];
    settings.iolevel = json_io_level[digitizer["io_level"].cast<int>()];
    
    settings.trig.sw = json_trig_mode[digitizer["sw_trig_mode"].cast<int>()];
    settings.trig.ext = json_trig_mode[digitizer["ext_trig_mode"].cast<int>()]; 
    settings.trig.dpp = json_dpp_trig_mode[digitizer["dpp_trig_mode"].cast<int>()];
    
    settings.trigholdoff = digitizer["trigger_holdoff"].cast<int>();
    
    settings.aggperblt = digitizer["aggregates_per_transfer"].cast<int>();
    
    for (int i = 0; i < settings.info.Channels; i++) {
        string chname = "CH["+to_string(i)+"]";
        if (db.find(chname) == db.end()) {
            settings.chans[i].enabled = false; 
        } else {
            json::Value &chan = db[chname];
            
            settings.chans[i].enabled = chan["enabled"].cast<bool>();
            
            if (settings.chans[i].enabled) { 
                settings.chans[i].samples = chan["total_samples"].cast<int>();
                settings.chans[i].presamples = chan["pretrig_samples"].cast<int>();
                settings.chans[i].offset = chan["offset"].cast<int>();
                
                settings.chans[i].selftrig = chan["self_trig"].cast<bool>();
                settings.chans[i].chargesens = chan["charge_sensitivity"].cast<int>();
                
                settings.chans[i].baseline = chan["baseline_flag"].cast<int>();
                
                settings.chans[i].threshold = chan["threshold"].cast<int>();
                
                settings.chans[i].pregate = chan["pregate"].cast<int>();
                settings.chans[i].shortgate = chan["shortgate"].cast<int>();
                settings.chans[i].longgate = chan["longgate"].cast<int>();
                
                settings.chans[i].coincidence = chan["coincidence"].cast<int>();
               
                settings.chans[i].eventsperagg = chan["events_per_aggregate"].cast<int>();
                
                settings.chans[i].trigmode = json_trig_mode[chan["trig_mode"].cast<int>()];
                settings.chans[i].pulsepol = json_pulse_polarity[chan["pulse_polarity"].cast<int>()]; 
            }
        }   
    }
}

void ApplySettings(int handle, Settings &settings) {
    if (settings.config_inter) 
        SAFE(CAEN_DGTZ_SetInterruptConfig(handle,settings.inter.state,settings.inter.level,settings.inter.status_id,settings.inter.event_number,settings.inter.mode));
    
    SAFE(CAEN_DGTZ_SetSWTriggerMode(handle,settings.trig.sw));
    SAFE(CAEN_DGTZ_SetExtTriggerInputMode(handle,settings.trig.ext));
    //SAFE(CAEN_DGTZ_SetDPPTriggerMode(handle,settings.trig.dpp));
    
    SAFE(CAEN_DGTZ_SetDPPAcquisitionMode(handle,settings.dppacqmode,settings.dppacqparam));
    
    SAFE(CAEN_DGTZ_SetRunSynchronizationMode(handle,settings.sync));
    SAFE(CAEN_DGTZ_SetIOLevel(handle,settings.iolevel));
    SAFE(CAEN_DGTZ_SetAcquisitionMode(handle, settings.acqmode));
    
    CAEN_DGTZ_DPP_PSD_Params_t params;
    params.trgho = settings.trigholdoff;
    
    int mask = 0;
    for (size_t i = 0; i < settings.info.Channels; i++) {
        if (settings.chans[i].enabled) {
            mask |= 1<<i;
            if (!(i % 2)) SAFE(CAEN_DGTZ_SetRecordLength(handle,settings.chans[i].samples,i)); //only valid for even channels in V1703 (FIXME more generic?)
            SAFE(CAEN_DGTZ_GetRecordLength(handle,&settings.chans[i].samples,i)); //update to actual value
            SAFE(CAEN_DGTZ_SetDPPPreTriggerSize(handle,i,settings.chans[i].presamples));
            SAFE(CAEN_DGTZ_GetDPPPreTriggerSize(handle,i,&settings.chans[i].presamples)); //update to actual value
            //SAFE(CAEN_DGTZ_SetNumEventsPerAggregate(handle,i,settings.chans[i].eventsperagg));
        
            params.thr[i] = settings.chans[i].threshold;
            params.selft[i] = settings.chans[i].selftrig ? 1 : 0;
            params.csens[i] = settings.chans[i].chargesens;
            params.sgate[i] = settings.chans[i].shortgate;
            params.lgate[i] = settings.chans[i].longgate;
            params.pgate[i] = settings.chans[i].pregate;
            params.tvaw[i] = settings.chans[i].coincidence;
            params.nsbl[i] = settings.chans[i].baseline;
            
            SAFE(CAEN_DGTZ_SetChannelSelfTrigger(handle,settings.chans[i].trigmode,1<<i));
            SAFE(CAEN_DGTZ_SetChannelPulsePolarity(handle,i,settings.chans[i].pulsepol));
            SAFE(CAEN_DGTZ_SetChannelDCOffset(handle,i,settings.chans[i].offset));
        
            //SAFE(CAEN_DGTZ_GetDPP_VirtualProbe(handle,i,));
        }
    }
    SAFE(CAEN_DGTZ_SetChannelEnableMask(handle,mask));
    SAFE(CAEN_DGTZ_SetDPPParameters(handle,mask,&params));
    
    SAFE(CAEN_DGTZ_SetMaxNumAggregatesBLT(handle,settings.aggperblt));
    
}
