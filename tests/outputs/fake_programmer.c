/* Only executable used by script tests. No USB/SWD/hardware library linked. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    const char *log = getenv("FAKE_PROGRAMMER_LOG");
    if (!log) return 90;
    FILE *f = fopen(log,"a"); if (!f) return 91;
    fputs("CALL\n",f);
    for(int i=1;i<argc;++i) fprintf(f,"ARG %s\n",argv[i]);
    fclose(f);
    int list = argc==2 && strcmp(argv[1],"-l")==0;
    const char *code = getenv(list ? "FAKE_LIST_CODE" : "FAKE_PROGRAM_CODE");
    const char *lines = getenv("FAKE_SERIAL_LINES");
    if(list) puts(lines ? lines : "ST-LINK SN : SNA\nST-LINK SN : SNB");
    else puts(getenv("FAKE_PROGRAM_ERROR") ? "Error: simulated failure" : "SIMULATED write/verify/reset OK");
    return code ? atoi(code) : 0;
}
