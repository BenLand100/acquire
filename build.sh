g++ -g -std=c++11 -DLINUX acquire.cc digitizer.cc json.cc -l hdf5_cpp -l hdf5 -l CAENDigitizer -l CAENVME -o acquire

g++ -g -std=c++11 -DLINUX trigrate.cc digitizer.cc json.cc -l ncurses -l CAENDigitizer -l CAENVME -o trigrate

g++ -g -std=c++11 -DLINUX v1718reset.c -l CAENVME -o v1718reset
