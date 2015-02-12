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
 
#include "acquire.hh"
#include "json.hh"

#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>
#include <map>

#include <unistd.h>

#include <H5Cpp.h>

using namespace H5;

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
            SAFE(CAEN_DGTZ_SetRecordLength(handle,settings.chans[i].samples,i));
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

int main(int argc, char **argv) {

    if (argc != 2) {
        cout << "./acquire settings.json" << endl;
        return -1;
    }
    
    cout << "Parsing settings..." << endl;
    
    map<string,json::Value> db = ReadDB(argv[1]);
    json::Value run = db["RUN[]"];
    
    const int ngrabs = run["events"].cast<int>();
    const string outfile = run["outfile"].cast<string>();
    const int transfer_wait = run["transfer_wait"].cast<int>();
    const int linknum = run["link_num"].cast<int>();
    const int baseaddr = run["base_address"].cast<int>();
    int nrepeat;
    if (run.isMember("repeat_times")) {
        nrepeat = run["repeat_times"].cast<int>();
    } else {
        nrepeat = 0;
    }
    
    
    for (int cycle = 0; cycle < nrepeat; cycle++) {
    
        cout << "Opening digitizer..." << endl;

        int handle; // CAENDigitizerSDK digitizer identifier
        SAFE(CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB, linknum, 0, baseaddr, &handle));
        SAFE(CAEN_DGTZ_SWStopAcquisition(handle));
        SAFE(CAEN_DGTZ_Reset(handle));
        
        Settings settings;
        InitSettings(handle,settings);
        SettingsFromDB(db,settings);
        
        cout << "Programming digitizer..." << endl;
        
        ApplySettings(handle,settings);
        
        cout << "Allocating readout buffers..." << endl;
        
        uint32_t size; 
        char *readout = NULL; // readout buffer (must init to NULL)
        uint32_t nevents[MAX_DPP_PSD_CHANNEL_SIZE]; // events read per channel
        CAEN_DGTZ_DPP_PSD_Event_t *events[MAX_DPP_PSD_CHANNEL_SIZE]; // event buffer per channel
        CAEN_DGTZ_DPP_PSD_Waveforms_t *waveform = NULL; // waveform buffer
        
        // ugh this syntax
        SAFE(CAEN_DGTZ_MallocReadoutBuffer(handle, &readout, &size));
        SAFE(CAEN_DGTZ_MallocDPPEvents(handle, (void**)events, &size));
        SAFE(CAEN_DGTZ_MallocDPPWaveforms(handle, (void**)&waveform, &size)); 
        
        cout << "Allocating temporary data storage..." << endl;
        
        map<int,int> chan2idx,idx2chan;
        vector<int> nsamples;
        vector<uint16_t*> grabs, baselines, qshorts, qlongs;
        vector<uint32_t*> times;
        for (size_t i = 0; i < settings.info.Channels; i++) {
            if (settings.chans[i].enabled) {
                chan2idx[i] = nsamples.size();
                idx2chan[nsamples.size()] = i;
                nsamples.push_back(settings.chans[i].samples);
                grabs.push_back(new uint16_t[ngrabs*nsamples.back()]);
                baselines.push_back(new uint16_t[ngrabs]);
                qshorts.push_back(new uint16_t[ngrabs]);
                qlongs.push_back(new uint16_t[ngrabs]);
                times.push_back(new uint32_t[ngrabs]);
            }
        }
        
        cout << "Starting acquisition " << cycle << "..." << endl;
        
        SAFE(CAEN_DGTZ_ClearData(handle));
        SAFE(CAEN_DGTZ_SWStartAcquisition(handle));
        vector<int> grabbed(chan2idx.size(),0);
        
        bool acquiring = true;
        while (acquiring) {
        
            cout << "Attempting readout...\n";
            
            //V1730 segfaults here for unknown reasons
            SAFE(CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, readout, &size)); //read raw data from the digitizer
            
            usleep(transfer_wait*1000);
            
            if (!size) continue;
            cout << "Transferred " << size << " bytes" << endl;
            
            SAFE(CAEN_DGTZ_GetDPPEvents(handle, readout, size, (void **)events, nevents)); //parses the buffer and populates events and nevents
            
            acquiring = false;
            
            for (uint32_t ch = 0; ch < settings.info.Channels; ch++) {
                if (!settings.chans[ch].enabled || grabbed[chan2idx[ch]] >= ngrabs) continue; //skip disabled channels
                
                acquiring = true;
                
                cout << "\t Ch" << ch << ": " << nevents[ch] << " events" << endl;
                
                int idx = chan2idx[ch];
                int &chgrabbed = grabbed[idx];
                
                for (uint32_t ev = 0; ev < nevents[ch] && chgrabbed < ngrabs; ev++, chgrabbed++) {
                    SAFE(CAEN_DGTZ_DecodeDPPWaveforms(handle, (void*) &events[ch][ev], (void*) waveform)); //unpacks the data into a nicer CAEN_DGTZ_DPP_PSD_Waveforms_t
                    /* FOR REFERENCE
                    typedef struct 
                    {
                        uint32_t Format;
                        uint32_t TimeTag;
	                    int16_t ChargeShort;
	                    int16_t ChargeLong;
                        int16_t Baseline;
	                    int16_t Pur;
                        uint32_t *Waveforms; 
                    } CAEN_DGTZ_DPP_PSD_Event_t;
                    typedef struct
                    {
                        uint32_t Ns;
                        uint8_t  dualTrace;
                        uint8_t  anlgProbe;
                        uint8_t  dgtProbe1;
                        uint8_t  dgtProbe2;
                        uint16_t *Trace1;
                        uint16_t *Trace2;
                        uint8_t  *DTrace1;
                        uint8_t  *DTrace2;
                        uint8_t  *DTrace3;
                        uint8_t  *DTrace4;
                    } CAEN_DGTZ_DPP_PSD_Waveforms_t;
                    */
                    memcpy(grabs[idx]+nsamples[idx]*chgrabbed,waveform->Trace1,sizeof(uint16_t)*nsamples[idx]);
                    baselines[idx][chgrabbed] = events[ch][ev].Baseline;
                    qshorts[idx][chgrabbed] = events[ch][ev].ChargeShort;
                    qlongs[idx][chgrabbed] = events[ch][ev].ChargeLong;
                    times[idx][chgrabbed] = events[ch][ev].TimeTag;
                }
            }
        }
        
        SAFE(CAEN_DGTZ_SWStopAcquisition(handle));
        SAFE(CAEN_DGTZ_CloseDigitizer(handle));
        
        Exception::dontPrint();
        
        string fname = outfile;
        if (nrepeat > 0) {
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
            
            Attribute bits = group.createAttribute("bits",PredType::NATIVE_UINT32,scalar);
            bits.write(PredType::NATIVE_INT32,&settings.info.ADC_NBits);
            
            Attribute ns_sample = group.createAttribute("ns_sample",PredType::NATIVE_DOUBLE,scalar);
            double val = 0.0;
            switch (settings.info.FamilyCode) {
                case 5:
                    val = 1.0;
                    break;
                case 11:
                    val = 2.0;
                    break;
            }
            ns_sample.write(PredType::NATIVE_DOUBLE,&val);
            
            Attribute offset = group.createAttribute("offset",PredType::NATIVE_UINT32,scalar);
            offset.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].offset);
            
            Attribute samples = group.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
            samples.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].samples);
            
            Attribute presamples = group.createAttribute("presamples",PredType::NATIVE_UINT32,scalar);
            presamples.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].presamples);
            
            Attribute threshold = group.createAttribute("threshold",PredType::NATIVE_UINT32,scalar);
            threshold.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].threshold);
            
            Attribute chargesens = group.createAttribute("chargesens",PredType::NATIVE_UINT32,scalar);
            chargesens.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].chargesens);
            
            Attribute baseline = group.createAttribute("baseline",PredType::NATIVE_UINT32,scalar);
            baseline.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].baseline);
            
            Attribute coincidence = group.createAttribute("coincidence",PredType::NATIVE_UINT32,scalar);
            coincidence.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].coincidence);
            
            Attribute shortgate = group.createAttribute("shortgate",PredType::NATIVE_UINT32,scalar);
            shortgate.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].shortgate);
            
            Attribute longgate = group.createAttribute("longgate",PredType::NATIVE_UINT32,scalar);
            longgate.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].longgate);
            
            Attribute pregate = group.createAttribute("pregate",PredType::NATIVE_UINT32,scalar);
            pregate.write(PredType::NATIVE_UINT32,&settings.chans[idx2chan[i]].pregate);
            
            hsize_t dimensions[2];
            dimensions[0] = ngrabs;
            dimensions[1] = nsamples[i];
            
            DataSpace samplespace(2, dimensions);
            DataSpace metaspace(1, dimensions);
            
            cout << "Samples, ";
            DataSet samples_ds = file.createDataSet(groupname+"/samples", PredType::NATIVE_UINT16, samplespace);
            samples_ds.write(grabs[i], PredType::NATIVE_UINT16);
            delete [] grabs[i];
            
            cout << "Baselines, ";
            DataSet baselines_ds = file.createDataSet(groupname+"/baselines", PredType::NATIVE_UINT16, metaspace);
            baselines_ds.write(baselines[i], PredType::NATIVE_UINT16);
            delete [] baselines[i];
            
            cout << "QShorts, ";
            DataSet qshorts_ds = file.createDataSet(groupname+"/qshorts", PredType::NATIVE_UINT16, metaspace);
            qshorts_ds.write(qshorts[i], PredType::NATIVE_UINT16);
            delete [] qshorts[i];
            
            cout << "QLongs, ";
            DataSet qlongs_ds = file.createDataSet(groupname+"/qlongs", PredType::NATIVE_UINT16, metaspace);
            qlongs_ds.write(qlongs[i], PredType::NATIVE_UINT16);
            delete [] qlongs[i];

            cout << "Times ";
            DataSet times_ds = file.createDataSet(groupname+"/times", PredType::NATIVE_UINT32, metaspace);
            times_ds.write(times[i], PredType::NATIVE_UINT32);
            delete [] times[i];
            
            cout << endl;
        }
    }
}
