/*
//                       SD-1000 MultiCART by Andrea Ottaviani 2024
//
//  SEGA SC-3000 - SG-1000  multicart based on Raspberry Pico board -
//
//  More info on https://github.com/aotta/ 
//
//   parts of code are directly from the A8PicoCart project by Robin Edwards 2023
//  
//   Needs to be a release NOT debug build for the cartridge emulation to work
// 
//   Edit myboard.h depending on the type of flash memory on the pico clone//
//
//   v. 1.0 2024-03-26 : Initial version for Pi Pico 
//
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "pico/divider.h"
#include "hardware/flash.h"
#include "hardware/sync.h"


#include "rom.h"

#include "tusb.h"
#include "ff.h"
#include "fatfs_disk.h"

// Pico pin usage definitions

#define A0_PIN    0
#define A1_PIN    1
#define A2_PIN    2
#define A3_PIN    3
#define A4_PIN    4
#define A5_PIN    5
#define A6_PIN    6
#define A7_PIN    7
#define A8_PIN    8
#define A9_PIN    9
#define A10_PIN  10
#define A11_PIN  11
#define A12_PIN  12
#define A13_PIN  13
#define A14_PIN  14
#define A15_PIN  15
#define D0_PIN   16
#define D1_PIN   17
#define D2_PIN   18
#define D3_PIN   19
#define D4_PIN   20
#define D5_PIN   21
#define D6_PIN   22
#define D7_PIN   23
#define MEMR_PIN  24
#define MEMW_PIN  25
#define MREQ_PIN  26
#define CEROM2_PIN  27
#define DSRAM_PIN  28 
#define IOR_PIN  29 

// Pico pin usage masks

#define A0_PIN_MASK     0x00000001L
#define A1_PIN_MASK     0x00000002L
#define A2_PIN_MASK     0x00000004L
#define A3_PIN_MASK     0x00000008L
#define A4_PIN_MASK     0x00000010L
#define A5_PIN_MASK     0x00000020L
#define A6_PIN_MASK     0x00000040L
#define A7_PIN_MASK     0x00000080L
#define A8_PIN_MASK     0x00000100L
#define A9_PIN_MASK     0x00000200L
#define A10_PIN_MASK    0x00000400L
#define A11_PIN_MASK    0x00000800L
#define A12_PIN_MASK    0x00001000L
#define A13_PIN_MASK    0x00002000L
#define A14_PIN_MASK    0x00004000L
#define A15_PIN_MASK    0x00008000L
#define D0_PIN_MASK     0x00010000L
#define D1_PIN_MASK     0x00020000L
#define D2_PIN_MASK     0x00040000L
#define D3_PIN_MASK     0x00080000L
#define D4_PIN_MASK     0x00100000L
#define D5_PIN_MASK     0x00200000L  // gpio 21
#define D6_PIN_MASK     0x00400000L
#define D7_PIN_MASK     0x00800000L

#define MEMR_PIN_MASK   0x01000000L //gpio 24
#define MEMW_PIN_MASK   0x02000000L
#define MREQ_PIN_MASK   0x04000000L  //gpio 26
#define CEROM2_PIN_MASK 0x08000000L
#define DSRAM_PIN_MASK  0x10000000L
#define IOR_PIN_MASK    0x20000000L

// Aggregate Pico pin usage masks
#define ALL_GPIO_MASK  	0x3FFFFFFFL
#define BUS_PIN_MASK    0x0000FFFFL
#define DATA_PIN_MASK   0x00FF0000L
#define FLAG_MASK       0x2F000000L
#define ROM_MASK ( MREQ_PIN_MASK  ) 
#define ALWAYS_IN_MASK  (BUS_PIN_MASK | FLAG_MASK)
#define ALWAYS_OUT_MASK (DATA_PIN_MASK | DOUTE_PIN_MASK)

#define SET_DATA_MODE_OUT   gpio_set_dir_out_masked(DATA_PIN_MASK)
#define SET_DATA_MODE_IN    gpio_set_dir_in_masked(DATA_PIN_MASK)
// We're going to erase and reprogram a region 256k from the start of flash.
// Once done, we can access this at XIP_BASE + 256k.


char RBLo,RBHi;
#define BINLENGTH  65536L
unsigned char ROM[BINLENGTH];
unsigned char files[256*256] = {0};
unsigned char nomefiles[32*25] = {0};
char curPath[256] = "";
char path[256];
int fileda=0,filea=0;
volatile char cmd=0;
char errorBuf[40];
bool cmd_executing=false;


/*
 Theory of Operation
 -------------------
 sega sends command to mcu on cart by writing to 50000 (CMD), 50001 (parameter) and menu (50002-50641) 
 sega must be running from RAM when it sends a command, since the mcu on the cart will
 go away at that point. Sega polls 50001 until it reads $1.
*/

void __not_in_flash_func(core1_main()) {

    uint32_t addr;
    char dataWrite=0;
    uint32_t pins;

	multicore_lockout_victim_init();	

   
    gpio_set_dir_in_masked(ALWAYS_IN_MASK);
    // Initial conditions
    SET_DATA_MODE_IN;
   
  while (1)
  {
    while ((pins=gpio_get_all()) & (MREQ_PIN_MASK)); //memr = b5 mreq=b10
	pins=gpio_get_all(); // re-read for SG-1000;
    addr = pins & BUS_PIN_MASK;
      if (!(pins & MEMR_PIN_MASK)) {
            SET_DATA_MODE_OUT;
            gpio_put_masked(DATA_PIN_MASK,ROM[addr]<<16);
         //   while (!(gpio_get_all() & MEMR_PIN_MASK));
            SET_DATA_MODE_IN;
          } else if (!(pins & (MEMW_PIN_MASK))) {        
            dataWrite=((gpio_get_all() & DATA_PIN_MASK) >> 16);
            ROM[addr]=dataWrite;
           // while (!(gpio_get_all() & MEMW_PIN_MASK));
          }
      } 
} 
    
////////////////////////////////////////////////////////////////////////////////////
//                     MENU Reset
////////////////////////////////////////////////////////////////////////////////////    

void reset() {
  multicore_lockout_start_blocking();	

 while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_OUT;
  gpio_put_masked(DATA_PIN_MASK,0xc7<<16);
  while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_IN;

   while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_OUT;
  gpio_put_masked(DATA_PIN_MASK,0xc7<<16);
  while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_IN;
  multicore_lockout_end_blocking();

  
   while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_OUT;
  gpio_put_masked(DATA_PIN_MASK,0xc7<<16);
  while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_IN;
}       



////////////////////////////////////////////////////////////////////////////////////
//                     Error(N)
////////////////////////////////////////////////////////////////////////////////////
void error(int numblink){
  while(1){
	gpio_set_dir(25,GPIO_OUT);
    
    for(int i=0;i<numblink;i++) {
      gpio_put(25,true);
      sleep_ms(600);
      gpio_put(25,false);
      sleep_ms(500);
    }
  sleep_ms(2000);
  }
}
////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	char isDir;
	char filename[13];
	char long_filename[32];
	char full_path[210];
} DIR_ENTRY;	// 256 bytes = 256 entries in 64k

int num_dir_entries = 0; // how many entries in the current directory

int entry_compare(const void* p1, const void* p2)
{
	DIR_ENTRY* e1 = (DIR_ENTRY*)p1;
	DIR_ENTRY* e2 = (DIR_ENTRY*)p2;
	if (e1->isDir && !e2->isDir) return -1;
	else if (!e1->isDir && e2->isDir) return 1;
	else return strcasecmp(e1->long_filename, e2->long_filename);
}

char *get_filename_ext(char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int is_valid_file(char *filename) {
	char *ext = get_filename_ext(filename);
	if (strcasecmp(ext, "BIN") == 0 || strcasecmp(ext, "ROM") == 0
		|| strcasecmp(ext, "SMS") == 0 
		|| strcasecmp(ext, "SG") == 0 || strcasecmp(ext, "SC") == 0)
		return 1;
	return 0;
}

FILINFO fno;
char search_fname[FF_LFN_BUF + 1];

// polyfill :-)
char *stristr(const char *str, const char *strSearch) {
    char *sors, *subs, *res = NULL;
    if ((sors = strdup (str)) != NULL) {
        if ((subs = strdup (strSearch)) != NULL) {
            res = strstr (strlwr (sors), strlwr (subs));
            if (res != NULL)
                res = (char*)str + (res - sors);
            free (subs);
        }
        free (sors);
    }
    return res;
}

int scan_files(char *path, char *search)
{
    FRESULT res;
    DIR dir;
    UINT i;

	res = f_opendir(&dir, path);
	if (res == FR_OK) {
		for (;;) {
			if (num_dir_entries == 255) break;
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) break;
			if (fno.fattrib & (AM_HID | AM_SYS)) continue;
			if (fno.fattrib & AM_DIR) {
				i = strlen(path);
				strcat(path, "/");
				if (fno.altname[0])	// no altname when lfn is 8.3
					strcat(path, fno.altname);
				else
					strcat(path, fno.fname);
				if (strlen(path) >= 210) continue;	// no more room for path in DIR_ENTRY
				res = scan_files(path, search);
				if (res != FR_OK) break;
				path[i] = 0;
			}
			else if (is_valid_file(fno.fname))
			{
				char *match = stristr(fno.fname, search);
				if (match) {
					DIR_ENTRY *dst = (DIR_ENTRY *)&files[0];
					dst += num_dir_entries;
					// fill out a record
					dst->isDir = (match == fno.fname) ? 1 : 0;	// use this for a "score"
					strncpy(dst->long_filename, fno.fname, 31);
					dst->long_filename[31] = 0;
					// 8.3 name
					if (fno.altname[0])
						strcpy(dst->filename, fno.altname);
					else {	// no altname when lfn is 8.3
						strncpy(dst->filename, fno.fname, 12);
						dst->filename[12] = 0;
					}
					// full path for search results
					strcpy(dst->full_path, path);

					num_dir_entries++;
				}
			}
		}
		f_closedir(&dir);
	}
	return res;
}

int search_directory(char *path, char *search) {
	char pathBuf[256];
	strcpy(pathBuf, path);
	num_dir_entries = 0;
	int i;
	FATFS FatFs;
	if (f_mount(&FatFs, "", 1) == FR_OK) {
		if (scan_files(pathBuf, search) == FR_OK) {
			// sort by score, name
			qsort((DIR_ENTRY *)&files[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
			DIR_ENTRY *dst = (DIR_ENTRY *)&files[0];
			// re-set the pointer back to 0
			for (i=0; i<num_dir_entries; i++)
				dst[i].isDir = 0;
			return 1;
		}
	}
	strcpy(errorBuf, "Problem searching flash");
	return 0;
}

int read_directory(char *path) {
	int ret = 0;
	num_dir_entries = 0;
	DIR_ENTRY *dst = (DIR_ENTRY *)&files[0];

    if (!fatfs_is_mounted())
       mount_fatfs_disk();

	FATFS FatFs;
	if (f_mount(&FatFs, "", 1) == FR_OK) {
		DIR dir;
		if (f_opendir(&dir, path) == FR_OK) {
			while (num_dir_entries < 255) {
				if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
					break;
				if (fno.fattrib & (AM_HID | AM_SYS))
					continue;
				dst->isDir = fno.fattrib & AM_DIR ? 1 : 0;
				if (!dst->isDir)
					if (!is_valid_file(fno.fname)) continue;
				// copy file record to first ram block
				// long file name
				strncpy(dst->long_filename, fno.fname, 31);
				dst->long_filename[31] = 0;
				// 8.3 name
				if (fno.altname[0])
		            strcpy(dst->filename, fno.altname);
				else {	// no altname when lfn is 8.3
					strncpy(dst->filename, fno.fname, 12);
					dst->filename[12] = 0;
				}
				dst->full_path[0] = 0; // path only for search results
	            dst++;
				num_dir_entries++;
			}
			f_closedir(&dir);
		}
		else
			strcpy(errorBuf, "Can't read directory");
		f_mount(0, "", 1);
		qsort((DIR_ENTRY *)&files[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
		ret = 1;
	}
	else
		strcpy(errorBuf, "Can't read flash memory");
	return ret;
}


/* load file in  ROM */

int load_file(char *filename) {
	FATFS FatFs;
	int car_file = 0;
	UINT br, size = 0;

	
	if (f_mount(&FatFs, "", 1) != FR_OK) {
		strcpy(errorBuf, "Can't read flash memory");
		return 0;
	}
	FIL fil;
	if (f_open(&fil, filename, FA_READ) != FR_OK) {
		strcpy(errorBuf, "Can't open file");
		goto cleanup;
	}


	// set a default error
	strcpy(errorBuf, "Can't read file");

	unsigned char *dst = &files[0];
	int bytes_to_read = 40 * 1024;
	// read the file to SRAM
	if (f_read(&fil, dst, bytes_to_read, &br) != FR_OK) {
		goto closefile;
	}
	size += br;

	
closefile:
	f_close(&fil);
cleanup:
	f_mount(0, "", 1);

	return br;
}

////////////////////////////////////////////////////////////////////////////////////
//                     filelist
////////////////////////////////////////////////////////////////////////////////////

void filelist(DIR_ENTRY* en,int da, int a)
{
  char longfilename[32];

  for(int i=0;i<32*20;i++) ROM[50002+i]=0;
    for(int n = 0;n<(a-da);n++) {
		memset(longfilename,0,32);
	
	 	if (en[n+da].isDir) {
			strcpy(longfilename,"DIR->");
			ROM[51000+n]=1;
			strcat(longfilename, en[n+da].long_filename);
	 	} else {
			ROM[51000+n]=0;
			strcpy(longfilename, en[n+da].long_filename);
	 	}
	 	for(int i=0;i<31;i++) {
      		ROM[50002+i+(n*32)]=longfilename[i];
	  		if ((ROM[50002+i+(n*32)])<=20) ROM[50002+i+(n*32)]=32;
     	}
		strcpy((char*)&nomefiles[32*n], longfilename);
	}
	ROM[51030]=da;ROM[51031]=a;ROM[51032]=num_dir_entries;
  }

////////////////////////////////////////////////////////////////////////////////////
//                     SEGAMenu
////////////////////////////////////////////////////////////////////////////////////
void SEGAMenu(int tipo) { // 1=start,2=next page, 3=prev page, 4=dir up
  int numfile=0;
  int maxfile=0;
  int ret=0;
  int rootpos[255];
  int lastpos;
  	
/////////////////// TIPO 1 /////////////////// 
  	if (tipo==1) {
		ret = read_directory(curPath);
		if (!(ret)) error(1);
		maxfile=20;
		fileda=0;
		if (maxfile>num_dir_entries) maxfile=num_dir_entries;
		filea=fileda+maxfile;
		filelist((DIR_ENTRY *)&files[0],fileda,filea);
		//sleep_ms(1400);
    } else 
/////////////////// TIPO 2 /////////////////// 
	if ((tipo==2) && (filea<num_dir_entries)) {
		maxfile=20;
		if ((filea+maxfile)>num_dir_entries) maxfile=num_dir_entries-filea;
   		fileda=filea;
		filea=fileda+maxfile;
		filelist((DIR_ENTRY *)&files[0],fileda,filea);
		//sleep_ms(3400);
		
	} else
/////////////////// TIPO 3 /////////////////// 
   	if ((tipo==3) && (fileda>=20)) {
		fileda=fileda-20;
		filea=fileda+20;
		filelist((DIR_ENTRY *)&files[0],fileda,filea);
	
	}
}
////////////////////////////////////////////////////////////////////////////////////
//                     Directory Up
////////////////////////////////////////////////////////////////////////////////////
void DirUp() {
	int len = strlen(curPath);
	if (len>0) {
		while (len && curPath[--len] != '/');
		curPath[len] = 0;
		//while (len && curPath[--len] != '/');
		//curPath[len] = 0;
	}

}
////////////////////////////////////////////////////////////////////////////////////
//                     LOAD Game
////////////////////////////////////////////////////////////////////////////////////
void LoadGame(){ 
  int numfile=0;
  int numErr=0;
  int romLen=0;
  char longfilename[32];
  
  char firstbyte=0x0;

    numfile=ROM[50001]+fileda-1;
	DIR_ENTRY *entry = (DIR_ENTRY *)&files[0];
    
	strcpy(longfilename,entry[numfile].long_filename);
    if (entry[numfile].isDir)
	{	// directory
     	strcat(curPath, "/");
		strcat(curPath, entry[numfile].filename);
		ROM[50000]=1; // re-read dir, path is changed
		SEGAMenu(1);
	} else {
		memset(path,0,sizeof(path));
		strcat(path,curPath);
		strcat(path, "/");
		strcat(path,longfilename);
   		for (int i=0;i<sizeof(path);i++) ROM[50002+i]=path[i];
		ROM[50000]=5;
		sleep_ms(540);
  		reset();
        load_file(path);  // load rom in files[]
        //load_file("/B/Bank Panic (JP).sg");
  		reset(); 
		memcpy(ROM,files,sizeof(ROM));
  		reset(); 
       while(1);    
  }
  
}
////////////////////////////////////////////////////////////////////////////////////
//                     Sega Cart Main
////////////////////////////////////////////////////////////////////////////////////

void sega_cart_main()
{
    uint32_t pins;
    uint32_t addr;
    uint32_t dataOut=0;
    uint16_t dataWrite=0;
 
 printf("Sega_cart_main\n");

	// overclocking isn't necessary for most functions - but XEGS carts weren't working without it
	// I guess we might as well have it on all the time.
  set_sys_clock_khz(250000, true);
  

    gpio_init_mask(ALL_GPIO_MASK);
  
    gpio_init(DSRAM_PIN);
    gpio_set_dir(DSRAM_PIN, GPIO_OUT);
    gpio_put(DSRAM_PIN, true);    

  stdio_init_all();   // for serial output, via printf()
  printf("Start\n");

  memcpy(ROM,_actest,sizeof(_actest));

  multicore_launch_core1(core1_main);
   
  // Initial conditions 
  curPath[0]=0;
  while (1) {
	 cmd_executing=false;
     cmd=ROM[50000];

     if ((cmd>0)&&!(cmd_executing)) {
      switch (cmd) {
      case 1:  // read file list
        cmd_executing=true;
        //sleep_ms(200);
	    ROM[50000]=0;
    	SEGAMenu(1);
	 	ROM[49999]=1;
      	break;
      case 2:  // run file list
        cmd_executing=true;
    	ROM[50000]=0;
	 	LoadGame();
		ROM[49999]=1;
      	break;
      case 3:  // next page
	    sleep_ms(200);
	    cmd_executing=true;
      	ROM[50000]=0;
     	SEGAMenu(2);
		ROM[49999]=1;
      	break;
      case 4:  // prev page
        cmd_executing=true;
      	ROM[50000]=0;
     	SEGAMenu(3);
		ROM[49999]=1;
      	break;
	  case 5:  // up dir
        cmd_executing=true;
      	sleep_ms(200);
	    ROM[50000]=0;
      	DirUp();
		SEGAMenu(1);
		ROM[49999]=1;
      	break;
    }     
   }
  }
}
 	
