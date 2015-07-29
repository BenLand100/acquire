#include <CAENVMElib.h>
#include <iostream>

int main(int argc, char **argv) {

    if (argc != 2) {
        std::cout << "./v1718reset [link number]" << std::endl;
        return -1;
    }

	int handle;
	CAENVME_Init(cvV1718,atoi(argv[1]),0,&handle);
	CAENVME_SystemReset(handle);
	CAENVME_End(handle);
	return 0;

}
