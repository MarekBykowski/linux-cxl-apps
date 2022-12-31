#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>

/*#include "cxl_mem_wrapper.h"*/
//#include <cxl_mem.h>

const char* help= "\
-h                           help message\n\
-query                       IOCTL CXL_MEM_QUERY_COMMANDS\n\
-cfg_rd [0xoffset]           IOCTL CXL_MEM_CONFIG_WR Read Hex\n\
-cfg_wr [0xoffset] [0xaddr]  IOCTL CXL_MEM_CONFIG_WR Write Hex\n\
-doe_discovery [0xindex=0-3] IOCTL CXL_MEM_CONFIG_WR Write Hex\n\
-doe_cxl [0xprotocol=0 or 2] [0xreq_code=0,1 for protocol=0]\n\
-doe_cma [0xnum = 0]         IOCTL CXL_MEM_CONFIG_WR Write Hex\n\
example:\n\
./cxl_app.exe -cfg_rd 0x00\n\
./cxl_app.exe -cfg_wr 0x10 0x00ff0004\n\
./cxl_app.exe -doe_discovery 0\n\
./cxl_app.exe -doe_cxl 2\n\
./cxl_app.exe -doe_cxl 0 1\n\
./cxl_app.exe -doe_cma 0\n\
  ";

#define READ  0
#define WRITE 1

int FD;
typedef struct cxl_pdev_config cxl_pdev_config;

int cxl_query()
{
     typedef struct cxl_mem_query_commands cxl_mem_query_commands;
     typedef struct cxl_command_info cxl_command_info;
     int n_cmds= 0;
     // QUERY with n_commands == 0 to get command size
     ioctl(FD, CXL_MEM_QUERY_COMMANDS, &n_cmds);
     printf("Querying\n");

     cxl_mem_query_commands* cmds= malloc(sizeof(cxl_mem_query_commands)
                                 + n_cmds * sizeof(cxl_command_info));
     cmds->n_commands= n_cmds;
     // QUERY with command size & pre-alloc memory
     ioctl(FD, CXL_MEM_QUERY_COMMANDS, cmds);

     for (int i= 0; i < (int)cmds->n_commands; i++) {
         printf(" id %d", cmds->commands[i].id);
         printf(" flags %d", cmds->commands[i].flags);
         printf(" size_in %d", cmds->commands[i].size_in);
         printf(" size_out %d\n", cmds->commands[i].size_out);
     }

     return 0;
};

int cxl_config(char* offset_s, char* data_s)
{
     int offset, data, is_write;
     cxl_pdev_config* config_payload= malloc(sizeof(cxl_pdev_config));
     if (data_s == NULL)
     	is_write= 0;
     else {
         is_write= 1;
         data= strtol(data_s, NULL, 16);
     }
     offset= strtol(offset_s, NULL, 16);

     config_payload->offset= offset;
     config_payload->data= data;
     config_payload->is_write= is_write;
     ioctl(FD, CXL_MEM_CONFIG_WR, config_payload);
     printf("CONFIG_WR %s [%0x] ", (is_write)? "write" : "read",
 		    config_payload->offset);
     for (int i= 0; i < 32; i += 8) printf(" %02x", (config_payload->data >> i) & 0xff);
     printf("\n");

     return 0;
};

void doe_config(cxl_pdev_config* config_payload, uint32_t offset, uint32_t data, uint32_t is_write) {
     config_payload->offset= offset;
     config_payload->data= data;
     config_payload->is_write= is_write;
     ioctl(FD, CXL_MEM_CONFIG_WR, config_payload);
     printf("CONFIG_%s [%0x] ", (is_write)? "WR": "RD",
 		    config_payload->offset);
     for (int i= 0; i < 32; i += 8) printf(" %02x", (config_payload->data >> i) & 0xff);
     printf("\n");
};

int cxl_doe_cxl(char* entry_s, char* data_s) {
     cxl_pdev_config* config_payload= malloc(sizeof(cxl_pdev_config));
     uint32_t data_obj[3];
     int length;
     uint32_t do_type;
     uint32_t req_type;

     do_type = strtol(entry_s, NULL, 16);
     req_type = strtol(data_s, NULL, 16);
     printf("DOE TYPE=%0x\n", do_type);
     printf("REQ TYPE=%0x\n", req_type);
     if (do_type == 0)
     	data_obj[0] = 0x00001e98;
     if (do_type == 2)
     	data_obj[0] = 0x00021e98;
     data_obj[1] = 0x3;
     data_obj[2] = req_type;
     printf("data_obj[0]=%x\n", data_obj[0]);
     printf("data_obj[1]=%x\n", data_obj[1]);
     printf("data_obj[2]=%x\n", data_obj[2]);

     printf("DOE\n");

     doe_config(config_payload, 0x170, data_obj[0], WRITE);
     doe_config(config_payload, 0x170, data_obj[1], WRITE);
     doe_config(config_payload, 0x170, data_obj[2], WRITE);

     /* Set GO */
     doe_config(config_payload, 0x168, 0x80000000, WRITE);
     /* check status READY is set 16c */
     doe_config(config_payload, 0x16c, 0, READ);

     /* read cdat response */
     doe_config(config_payload, 0x174, 0, READ);

     /* write cdat response success */
     doe_config(config_payload, 0x174, 0x00000001, WRITE);

     /* read cdat response */
     doe_config(config_payload, 0x174, 0, READ);

     length = config_payload->data & 0x0000ffff;
     printf("DOE RSP LENGTH = %0d\n", length);

     /* write cdat response success */
     doe_config(config_payload, 0x174, 0x00000001, WRITE);

     for (int j= 0; j < length-2; j=j+1) {
         /* read cdat response */
     	doe_config(config_payload, 0x174, 0, READ);

         /* write cdat response success */
     	doe_config(config_payload, 0x174, 0x00000001, WRITE);
     }
     return 0;
};

int cxl_doe_cma(char* entry_s, char* data_s) {
     cxl_pdev_config* config_payload= malloc(sizeof(cxl_pdev_config));
     uint32_t data_obj[3];

     data_obj[0] = 0x00010001;
     data_obj[1] = 0x3;
     data_obj[2] = strtol(entry_s, NULL, 16);

     printf("DOE\n");
     doe_config(config_payload, 0x170, data_obj[0], WRITE);
     doe_config(config_payload, 0x170, data_obj[1], WRITE);
     doe_config(config_payload, 0x170, data_obj[2], WRITE);

     /* Set GO */
     doe_config(config_payload, 0x168, 0x80000000, WRITE);

     /* check status READY is set 16c */
     doe_config(config_payload, 0x16c, 0, READ);

     /* read Discovery response */
     doe_config(config_payload, 0x174, 0, READ);

     doe_config(config_payload, 0x168, 0x1, WRITE);

     /* check status READY is set 16c */
     doe_config(config_payload, 0x16c, 0, READ);
};

int cxl_doe_discovery(char* entry_s, char* data_s) {
     cxl_pdev_config* config_payload= malloc(sizeof(cxl_pdev_config));
     uint32_t data_obj[3];

     data_obj[0] = 0x00000001;
     data_obj[1] = 0x3;
     data_obj[2] = strtol(entry_s, NULL, 16);

     printf("DOE\n");
     doe_config(config_payload, 0x170, data_obj[0], WRITE);
     doe_config(config_payload, 0x170, data_obj[1], WRITE);
     doe_config(config_payload, 0x170, data_obj[2], WRITE);

     /* Set GO */
     doe_config(config_payload, 0x168, 0x80000000, WRITE);

     /* check status READY is set 16c */
     doe_config(config_payload, 0x16c, 0, READ);

     /* read Discovery response */
     doe_config(config_payload, 0x174, 0, READ);

     /* write Discovery response success */
     doe_config(config_payload, 0x174, 0x00000001, WRITE);

     doe_config(config_payload, 0x174, 0, READ);

     /* write Discovery response success */
     doe_config(config_payload, 0x174, 0x00000002, WRITE);

     doe_config(config_payload, 0x174, 0, READ);

     /* write Discovery response success */
     doe_config(config_payload, 0x174, 0x00000003, WRITE);

     return 0;
};

int parse_input(int argc, char** argv) {
     int idx;
     if (argc < 2) return -1;
     for (idx= 0; idx < argc; idx++) {
         if (strcmp(argv[idx], "-h") == 0)
             return -1;
         if (strcmp(argv[idx], "-query") == 0)
             return cxl_query();
         if (strcmp(argv[idx], "-cfg_rd") == 0)
             return cxl_config(argv[idx + 1], NULL);
         if (strcmp(argv[idx], "-cfg_wr") == 0)
             return cxl_config(argv[idx + 1], argv[idx + 2]);
         if (strcmp(argv[idx], "-doe_discovery") == 0)
             return cxl_doe_discovery(argv[idx + 1], NULL);
         if (strcmp(argv[idx], "-doe_cxl") == 0)
             return cxl_doe_cxl(argv[idx + 1], argv[idx + 2]);
         if (strcmp(argv[idx], "-doe_cma") == 0)
             return cxl_doe_cma(argv[idx + 1], NULL);
     }
     return 0;
};

int main(int argc, char** argv) {
     int ret;
     char* dev_path= "/dev/cxl/mem0";

     if ((FD= open(dev_path, O_RDWR)) < 0) {
         printf("Open error loc: %s\n", dev_path);
         printf("Try sudo %s\n", argv[0]);
         exit(0);
     }

     if ((ret= parse_input(argc, argv)) < 0) {
         printf("Please specify input ");
         for (int i= 0; i < argc; i++) printf(" %s", argv[i]);;
           printf("\n%s\n", help);
     }
     printf("DOE\n");

     close(FD);
     exit(0);
}
