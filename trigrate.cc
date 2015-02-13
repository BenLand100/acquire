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
 
#include "digitizer.hh"

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <sys/time.h> 

#include <curses.h>

using namespace std;

int main(int argc, char **argv) {

    if (argc != 2) {
        cout << "./trigrate settings.json" << endl;
        return -1;
    }
    
    cout << "Parsing settings..." << endl;
    
    map<string,json::Value> db = ReadDB(argv[1]);
    json::Value run = db["RUN[]"];
    
    const int transfer_wait = run["transfer_wait"].cast<int>();
    const int update_wait = run["update_wait"].cast<int>();
    const int linknum = run["link_num"].cast<int>();
    const int baseaddr = run["base_address"].cast<int>();

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
    for (size_t i = 0; i < settings.info.Channels; i++) {
        if (settings.chans[i].enabled) {
            int idx = chan2idx.size();
            chan2idx[i] = idx;
            idx2chan[idx] = i;
        }
    }
    vector<size_t> sumevents(chan2idx.size(),0);

    cout << "Starting trigrate..." << endl;
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    
    move(0,0);
    addstr("Press any key to exit ");

    SAFE(CAEN_DGTZ_ClearData(handle));
    SAFE(CAEN_DGTZ_SWStartAcquisition(handle));
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    while (true) {
    
        int ch;
        if ((ch = getch()) != ERR) {
            break;
        }

        SAFE(CAEN_DGTZ_ReadData(handle, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, readout, &size)); //read raw data from the digitizer
        
        usleep(transfer_wait*1000);
        
        if (!size) continue;
        
        SAFE(CAEN_DGTZ_GetDPPEvents(handle, readout, size, (void **)events, nevents)); //parses the buffer and populates events and nevents
        
        gettimeofday(&end, NULL);
        size_t elapsed = ((end.tv_sec - start.tv_sec)*1000  + (end.tv_usec - start.tv_usec)/1000);
        if (elapsed >= update_wait) start = end;
        
        for (uint32_t ch = 0; ch < settings.info.Channels; ch++) {
            if (!settings.chans[ch].enabled) continue; //skip disabled channels
            
            int idx = chan2idx[ch];
            sumevents[idx] += nevents[ch];
            
            string evt = "Ch" + to_string(ch) + ": " + to_string(nevents[ch]) + " events/acq            ";
            move(idx*2+2,0);
            addstr(evt.c_str());
            
            if (elapsed >= update_wait) {
                string rte = "     " + to_string((double)sumevents[idx]/(double)elapsed*1000.0) + " Hz       ";
                sumevents[idx] = 0;
                move(idx*2+3,0);
                addstr(rte.c_str());
            }
        }
    }

    SAFE(CAEN_DGTZ_SWStopAcquisition(handle));
    SAFE(CAEN_DGTZ_CloseDigitizer(handle));
    
    endwin();

}
