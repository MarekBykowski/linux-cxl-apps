#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include "include/linux/cxl_mem.h"  /* ioctl symbols, structs */
#include "include/linux/pci_regs.h" /* bitfield mask, etc.*/

#include <mbox.h>
#include <doe.h>
#include <bitfield.h>
#include <debug_or_not.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

/*
 * To understand the IOCTL code/define from cxl_mem.h, eg.
 *
 *   #define CXL_MEM_QUERY_COMMANDS _IOR(0xCE, 1, struct cxl_mem_query_commands)
 *   #define CXL_MEM_SEND_COMMAND _IOWR(0xCE, 2, struct cxl_send_command)
 *
 * go to Documentation/userspace-api/ioctl/ioctl-decoding.rst.
 *
 * To understand what the _IO, _IOW, _IOR, _IORW are go to
 * Documentation/userspace-api/ioctl/ioctl-number.rst. In brief, these are
 * the macros that take three? args:
 * - 1st arg is a identifying letter
 * - 2nd -//- is a sequence number
 * - 3rd (and last) is a type of the data going into or coming out of the kernel
 *
 * For CXL it is (excerpt from the doc):
 * ====  =====  ====================== ======================
 * Code  Seq#   Include File           Comments
 * ====  =====  ====================== ======================
 * [...]
 * 0xCE  01-02  uapi/linux/cxl_mem.h   Compute Express Link
 * [...]
 *
 * However it is rather impossible to include the cxl_mem.h without the changes.
 * It shall be used by the libraries, eg. glibc and not directly by the apps.
 * However because installing/updating the libs may be impossible without
 * the root access or cumbersome in other cases, there are ways to use
 * the header file directly.
 * See https://github.com/MarekBykowski/readme/wiki/kernel-headers-for-svc
 */

/*
 *  Throughout this unit the following format of a field value is employed:
 *   - binary 8'b10110100 -> 8 bits of value 10110100
 *   - or binary b10110100 -> just the value 10110100
 *   - hex 0x00b4
 *  that all the three equal to 180.
 */

const char* help= "\
-h                           help message\n\
-query                       CXL_MEM_QUERY_COMMANDS\n\
-cfg_rd [0xoffset]           CXL_MEM_CONFIG_WR Read Hex\n\
-cfg_wr [0xoffset] [0xaddr]  CXL_MEM_CONFIG_WR Write Hex\n\
-doe_discovery [0xindex=0-3] CXL_DISCOVERY\n\
-doe_cxl_cdat_get_length     CDAT length\n\
-doe_cxl_cdat_read_table     Prints all the CDAT tables\n\
./cxl_app -doe_cxl_compliance Request/Response Code is from 0 thr 0xf\n\
Note: you always read/write from/to offset from the DOE instance and not the config space\n\
example:\n\
./cxl_app -cfg_rd 0x00\n\
./cxl_app -cfg_wr 0x10 0x00ff0004\n\
./cxl_app -doe_discovery 0\n\
./cxl_app -doe_cxl_cdat_get_length\n\
./cxl_app -doe_cxl_cdat_read_table\n\
./cxl_app -doe_cxl_complience 0xf\n\
  ";

#define READ  0
#define WRITE 1

int FD;
typedef struct cxl_pdev_config cxl_pdev_config;

int cxl_query(void)
{
	typedef struct cxl_mem_query_commands cxl_mem_query_commands;
	typedef struct cxl_command_info cxl_command_info;
	int n_cmds= 0;

	/* QUERY with n_commands == 0 to get command size */
	ioctl(FD, CXL_MEM_QUERY_COMMANDS, &n_cmds);
	printf("Querying\n");

	cxl_mem_query_commands* cmds= malloc(sizeof(cxl_mem_query_commands)
			 + n_cmds * sizeof(cxl_command_info));
	cmds->n_commands = n_cmds;

	/* QUERY with command size & pre-alloc memory */
	ioctl(FD, CXL_MEM_QUERY_COMMANDS, cmds);

	for (int i = 0; i < (int)cmds->n_commands; i++) {
		printf("cmd[%d]=%s", i, cxl_mem_id_to_name(cmds->commands[i].id));
		printf("\t-> @flags %d", cmds->commands[i].flags);
		printf(" @size_in %d", cmds->commands[i].size_in);
		printf(" @size_out %d\n", cmds->commands[i].size_out);
	}

	{
	/*
	 * @id: ID number for the command 1
	 * @flags: Flags that specify command behavior 0
	 * @size_in: Expected input size, or ~0 if variable length 0
	 * @size_out: Expected output size, or ~0 if variable length 67
	 */
	char payload_out[67] = {0};
	struct cxl_send_command *csc = malloc(sizeof(struct cxl_send_command));
	csc->id = 1;
	csc->flags = 0;
	csc->in.size = 0;
	csc->in.payload = 0;
	csc->out.size = 67;
	csc->out.payload = (unsigned long)&payload_out;
	ioctl(FD, CXL_MEM_SEND_COMMAND, csc);
	printf("cmd=%s result=%s\n", cxl_mem_id_to_name(csc->id), (char *)csc->out.payload);
	}

	return 0;
};

int cxl_config(char* offset_s, char* data_s)
{
	int offset, val, is_write;
	cxl_pdev_config *config_payload = malloc(sizeof(cxl_pdev_config));

	if (data_s == NULL)
		is_write = 0;
	else {
		is_write = 1;
		val = strtol(data_s, NULL, 16);
	}

	offset = strtol(offset_s, NULL, 16);

	config_payload->offset = offset;
	config_payload->val = val;
	config_payload->is_write = is_write;

	ioctl(FD, CXL_MEM_CONFIG_WR, config_payload);

	printf("CONFIG_WR %s [%0x] ", (is_write)? "write" : "read",
		    config_payload->offset);

	for (int i = 0; i < 32; i += 8)
		printf(" %02x", (config_payload->val >> i) & 0xff);

	printf("\n");
	return 0;
};

void doe_config(cxl_pdev_config* config_payload, uint32_t offset,
		uint32_t val, uint32_t is_write)
{
	int i;
	config_payload->offset = offset;
	config_payload->val = val;
	config_payload->is_write = is_write;

	ioctl(FD, CXL_MEM_CONFIG_WR, config_payload);

	printf("CONFIG_%s [%0x] ", is_write ? "WR": "RD", config_payload->offset);
	printf(" %08x ", config_payload->val);

	for (i = 0; i < 32; i += 8)
		print_by_byte(" %02x", (config_payload->val >> i) & 0xff);

	printf("\n");
};

int cxl_doe_discovery(char* dword_s)
{
	/*
	 *  #### DW0 - Header1 ####
	 *  [31:24]	Resv			don't care
	 *  [23:16]	Data Object Type	0x0
	 *  [15:0]	Vendor ID		0x0001
	 *
	 *  #### DW1 - Header2 ####
	 *  [31:18]	Resv			-//-
	 *  [17:0]	Length			0x3
	 *
	 *  #### DW2 Request (Data Object DWORD 0) ####
	 *  [31:0]	Index			0, 1, then 2, etc.
	 *					until DW Response[31:24]
	 *					returns 0 indicating it's final entry.
	 *
	 *  ...or...
	 *
	 *  #### DW2 Response (-//-). Note, response is also followed by two headers ####
	 *  [31:24]	Next Indext		?
	 *  [23:16]	Data Object Type	?
	 *  [15:0]	Vendor ID		?
	 */

	cxl_pdev_config* config_payload = malloc(sizeof(cxl_pdev_config));
	uint32_t data_obj[3];

	data_obj[0] = 0x00000001;
	data_obj[1] = 0x3;
	data_obj[2] = strtol(dword_s, NULL, 16);

	pr_debug("Issue Abort\n");
	doe_config(config_payload, PCI_DOE_CTRL, PCI_DOE_CTRL_ABORT, WRITE);

	pr_debug("Write DOE header1 (vid and type)\n");
	doe_config(config_payload, PCI_DOE_WRITE, data_obj[0], WRITE);

	pr_debug("Write DOE header2 (length)\n");
	doe_config(config_payload, PCI_DOE_WRITE, data_obj[1], WRITE);

	pr_debug("Write DWORD (index=%s)\n", type_s);
	doe_config(config_payload, PCI_DOE_WRITE, data_obj[2], WRITE);

	pr_debug("Set GO\n");
	doe_config(config_payload, PCI_DOE_CTRL, PCI_DOE_CTRL_GO, WRITE);

	pr_debug("Check Data Object Ready is set?\n");
	doe_config(config_payload, PCI_DOE_STATUS, 0, READ);

	/*
	 * Read the first dword to get the protocol. We expect VID, and
	 * the Protocol to be 0x1 and 0x0 respectively.
	 */
	doe_config(config_payload, PCI_DOE_READ, 0, READ);
	/* Write Discovery response success */
	doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);

	/*
	 * Read the second dword to get the length. As we will read only
	 * the next dword afterwards we assume it is three dword long.
	 *
	 * Note, DW Response[31:24] may have Next Index value set, indicating
	 * there may be a pair of VID-Protocol for CMA/SPDM, and then for
	 * SCMA/SPDM supported from the DOE instance.
	 */
	doe_config(config_payload, PCI_DOE_READ, 0, READ);
	/* Write Discovery response success */
	doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);

	doe_config(config_payload, PCI_DOE_READ, 0, READ);
	/* Write Discovery response success */
	doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);

	return 0;
};

int cxl_doe_cxl_cdat(char *dword_s, char *length_or_table)
{
	cxl_pdev_config* config_payload = malloc(sizeof(cxl_pdev_config));
	uint32_t data_obj[3], dword;
	int length, payload_length, i;
	int entry_handle = 0;

	dword = strtol(dword_s, NULL, 16);
	printf("DOE TYPE=2 VID=0x1e98\n");
	printf("DWORD REQUEST (EntryHandle)=%x\n", dword);

	data_obj[0] = 0x00021e98;
	data_obj[1] = 0x3;
	data_obj[2] = dword;

	do {
		u32 cdat_length;

		pr_debug("Issue Abort\n");
		doe_config(config_payload, PCI_DOE_CTRL, PCI_DOE_CTRL_ABORT, WRITE);

		pr_debug("Write DOE header1 (vid and type)\n");
		doe_config(config_payload, PCI_DOE_WRITE, data_obj[0], WRITE);

		pr_debug("Write DOE header2 (length)\n");
		doe_config(config_payload, PCI_DOE_WRITE, data_obj[1], WRITE);

		pr_debug("Write DWORD (%04x)\n", dword);
		doe_config(config_payload, PCI_DOE_WRITE, data_obj[2], WRITE);

		pr_debug("Set GO\n");
		doe_config(config_payload, PCI_DOE_CTRL, PCI_DOE_CTRL_GO, WRITE);

		pr_debug("Check Data Object Ready is set?\n");
		doe_config(config_payload, PCI_DOE_STATUS, 0, READ);
		if (!FIELD_GET(PCI_DOE_STATUS_DATA_OBJECT_READY, config_payload->val))
			printf("Data Object Ready Clear - Error\n");

		/* Read the first dword to get the header1 */
		doe_config(config_payload, PCI_DOE_READ, 0, READ);
		/* Write anything to indicate success */
		doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);

		/* Read the second word to get the length */
		doe_config(config_payload, PCI_DOE_READ, 0, READ);
		length = config_payload->val & 0x0000ffff;
		payload_length = length - 2;
		payload_length = max(payload_length, 0);
		/* Write to indicate success */
		doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);

		printf("DOE response length=%0d response payload length %0d\n",
		       length, payload_length);

		for (i = 0; i < payload_length; i++) {
			/* Read cdat response */
			doe_config(config_payload, PCI_DOE_READ, 0, READ);

			/*
			 * Get the CXL table access header entry handle.
			 * entry handle 0xffff_xxxx indicates no more entries
			 */
			if (i == 0) {
				entry_handle = FIELD_GET(CXL_DOE_TABLE_ACCESS_ENTRY_HANDLE,
							 config_payload->val);
				pr_debug("entry_handle %08x\n", entry_handle);
			}

			if (i == 1) {
				cdat_length = config_payload->val;
			}


			/* Prior to the last ack, ensure Data Object Ready */
			if (i == payload_length - 1) {
				doe_config(config_payload, PCI_DOE_STATUS, 0, READ);
				if (!FIELD_GET(PCI_DOE_STATUS_DATA_OBJECT_READY, config_payload->val))
					printf("Data Object Ready Clear - Error\n");
			}

			/* Write to advance on */
			doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);
		}

		data_obj[2] += 0x10000;

		if (0 == strncmp("length", length_or_table, sizeof("length"))) {
			printf("CDAT length %08x\n",  cdat_length);
			break;
		} else if (0 == strncmp("table", length_or_table, sizeof("table")))
			printf("\n");

	/* Iterate until handle_entry 0xffffxxxx for no more entires. */
	} while (entry_handle != CXL_DOE_TABLE_ACCESS_LAST_ENTRY);

	return 0;
};

int cxl_doe_cxl_compliance(char *dword_s)
{
	cxl_pdev_config* config_payload = malloc(sizeof(cxl_pdev_config));
	uint32_t data_obj[3], dword;
	int length, payload_length, i;

	dword = strtol(dword_s, NULL, 16);
	printf("DOE TYPE=0 VID=0x1e98\n");
	printf("DWORD REQUEST (Version of Capability Requested)=0x%02x\n", dword);

	data_obj[0] = 0x00001e98;
	data_obj[1] = 0x3;
	data_obj[2] = dword;

	pr_debug("Issue Abort\n");
	doe_config(config_payload, PCI_DOE_CTRL, PCI_DOE_CTRL_ABORT, WRITE);

	pr_debug("Write DOE header1 (vid and type)\n");
	doe_config(config_payload, PCI_DOE_WRITE, data_obj[0], WRITE);

	pr_debug("Write DOE header2 (length)\n");
	doe_config(config_payload, PCI_DOE_WRITE, data_obj[1], WRITE);

	pr_debug("Write DWORD (index=%s)\n", type_s);
	doe_config(config_payload, PCI_DOE_WRITE, data_obj[2], WRITE);

	pr_debug("Set GO\n");
	doe_config(config_payload, PCI_DOE_CTRL, PCI_DOE_CTRL_GO, WRITE);

	pr_debug("Check Data Object Ready is set?\n");
	doe_config(config_payload, PCI_DOE_STATUS, 0, READ);

	/* Read the first dword to get the header1 */
	doe_config(config_payload, PCI_DOE_READ, 0, READ);
	/* Write anything to indicate success */
	doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);

	/* Read the second word to get the length */
	doe_config(config_payload, PCI_DOE_READ, 0, READ);
	length = config_payload->val & 0x0000ffff;
	payload_length = length - 2;
	payload_length = max(payload_length, 0);

	/* Write to indicate success */
	doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);

	printf("DOE response length=%0d response payload length %0d\n",
	       length, payload_length);

	for (i = 0; i < payload_length; i++) {
		doe_config(config_payload, PCI_DOE_READ, 0, READ);

		/* Prior to the last ack, ensure Data Object Ready */
		if (i == payload_length - 1) {
			doe_config(config_payload, PCI_DOE_STATUS, 0, READ);
			if (!FIELD_GET(PCI_DOE_STATUS_DATA_OBJECT_READY, config_payload->val))
				printf("Data Object Ready Clear - Error\n");
		}

		/* Write to advance on */
		doe_config(config_payload, PCI_DOE_READ, 0x0, WRITE);
	}

	return 0;
}

int parse_input(int argc, char **argv)
{
	int idx;

	if (argc < 2)
		return -1;

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
			return cxl_doe_discovery(argv[idx + 1]);
		if (strcmp(argv[idx], "-doe_cxl_cdat_get_length") == 0)
			return cxl_doe_cxl_cdat("0", "length");
		if (strcmp(argv[idx], "-doe_cxl_cdat_read_table") == 0)
			return cxl_doe_cxl_cdat("0", "table");
		if (strcmp(argv[idx], "-doe_cxl_complience") == 0)
			return cxl_doe_cxl_compliance(argv[idx + 1]);
	}
	return 0;
};

int main(int argc, char** argv)
{
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

     close(FD);
     exit(0);
}
