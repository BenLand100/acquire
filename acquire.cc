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

#include <unistd.h>

#include <H5Cpp.h>

using namespace H5;

using namespace std;

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
    
    
    for (int cycle = nrepeat ? 0 : -1; cycle < nrepeat; cycle++) {
    
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
        CAEN_DGTZ_DPP_PSD_Event_t *events[16]; // event buffer per channel
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
        
        if (cycle >= 0) {
            cout << "Starting acquisition " << cycle << "..." << endl;
        } else {
            cout << "Starting acquisition..." << endl;
        }
        
        SAFE(CAEN_DGTZ_ClearData(handle));
        SAFE(CAEN_DGTZ_SWStartAcquisition(handle));
        vector<int> grabbed(chan2idx.size(),0);
        
        bool acquiring = true;
        while (acquiring) {
        
            cout << "Attempting readout...\n";
            
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
