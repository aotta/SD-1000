/*
//                       SD-1000 MultiCART by Andrea Ottaviani 
// SEGA SC-3000 - SG-1000  multicart based on Raspberry Pico board -

// v. 1.0 2024-03-26 : Initial version for Pi Pico 
//
// 
//  More info on https://github.com/aotta/ 
*/

#include "hardware/gpio.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "string.h"
#include "splash.h"

#include "dir_entry.hpp"
// include for Flash files
#include "SdFat_Adafruit_Fork.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"

Adafruit_FlashTransport_RP2040 flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

FatVolume fatfs;
File32 root;
File32 file;
File32 romFile;

Adafruit_USBD_MSC usb_msc;
bool fs_formatted;
bool fs_changed = false;


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
//#define ROM_MASK ( MREQ_PIN_MASK  ) // | MREQ_PIN_MASK | MEMR_PIN_MASK A15_PIN_MASK |
#define ROM_MASK ( MREQ_PIN_MASK  | MREQ_PIN_MASK | MEMR_PIN_MASK | A15_PIN_MASK)
#define ALWAYS_IN_MASK  (BUS_PIN_MASK | FLAG_MASK)

#define SET_DATA_MODE_OUT   gpio_set_dir_out_masked(DATA_PIN_MASK)
#define SET_DATA_MODE_IN    gpio_set_dir_in_masked(DATA_PIN_MASK)
// We're going to erase and reprogram a region 256k from the start of flash.
// Once done, we can access this at XIP_BASE + 256k.


char RBLo,RBHi;
#define BINLENGTH  65536L
unsigned char ROM[BINLENGTH];
unsigned char game[BINLENGTH];
unsigned char files[256*256] = {0};
unsigned char nomefiles[32*25] = {0};
char curPath[256] = "";
char path[256];
int fileda=0,filea=0;
bool cmd_executing=false;

// Callbacks USB MSC
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  return flash.readBlocks(lba, (uint8_t*)buffer, bufsize / 512) ? bufsize : -1;
}

int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}

void msc_flush_cb(void) {
  flash.syncBlocks();
  fatfs.cacheClear();
  fs_changed = true;
}


////////////////////////////////////////////////////////////////////////////////////
//                     REBOOT
////////////////////////////////////////////////////////////////////////////////////
void doReboot() {
  rp2040.reboot();
}
////////////////////////////////////////////////////////////////////////////////////
//                     HANDLE BUS
////////////////////////////////////////////////////////////////////////////////////

void __time_critical_func(loop1()) {   //HandleBUS()
  char dataWrite=0;
  uint32_t pins=0;
  uint32_t addr;
  
  // Main loop   
   
  gpio_set_dir_in_masked(ALWAYS_IN_MASK);
  
  // Initial conditions
  SET_DATA_MODE_IN;
    
    
  while (1)
 {
    while ((pins = gpio_get_all()) & MREQ_PIN_MASK ); //removed for compatibility with SG-1000, which doesn't have IOR
   
    pins = gpio_get_all();   // to keep for SG-1000 compatibility, we read the pins again after the wait for MREQ to go low
	
    addr = pins & BUS_PIN_MASK;

    if (!(pins & MEMR_PIN_MASK)) {
        SET_DATA_MODE_OUT;
		    gpio_put_masked(DATA_PIN_MASK, ROM[addr] << 16);  
        // Sync: wait end of read from CPU Z80
        while (!(gpio_get_all() & MEMR_PIN_MASK));  // removed for compatibility with Mark III
        SET_DATA_MODE_IN;
    } 
    else if (!(pins & MEMW_PIN_MASK)) {        
		if ((1)) { //little delay to fix Golgo 13 & Ninja Princess
	    dataWrite = ((gpio_get_all() & DATA_PIN_MASK) >> 16);
      ROM[addr] = dataWrite;
      // Sync: wait end of write from CPU Z80
			while (!(gpio_get_all() & MEMW_PIN_MASK)); 
		}
	}
 }
}
////////////////////////////////////////////////////////////////////////////////////
//                     RESET SEGA
////////////////////////////////////////////////////////////////////////////////////    
void reset() {
  
  rp2040.idleOtherCore();
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

  
   while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_OUT;
  gpio_put_masked(DATA_PIN_MASK,0xc7<<16);
  while (!(gpio_get_all() & MEMR_PIN_MASK));
  SET_DATA_MODE_IN;
  rp2040.resumeOtherCore();
}       

////////////////////////////////////////////////////////////////////////////////////
//                     LOAD ROM
////////////////////////////////////////////////////////////////////////////////////
void loadROM(){ 
  String riga;
  int numErr=0;
  
  for(int i=0;i<(sizeof(_actest));i++){
    unsigned int dataW= _actest[i];
    ROM[i]=dataW;
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
	
   // vreg_set_voltage(VREG_VOLTAGE_1_20); // set to 1_15 or 1_20 if you experience some glitches
   // sleep_ms(10);
   // set_sys_clock_khz(270000, true); // settled in compiler IDE as 250mhz overclocked
  
    reset(); 
   	memcpy(ROM,game,BINLENGTH);
	  
  
    reset(); 
    
    while(1);    
  }  
}

/////////////////////////////// file structures 

int read_directory(char *path) {
  //  if (path[0]==0) strcpy(path,'/');
    Serial.print("Read_directory: ");
    Serial.println(path);
    int ret = 0;
    num_dir_entries = 0;
    DIR_ENTRY *dst = (DIR_ENTRY *)&files[0];

    FatFile dir;
    if (dir.open(path, O_RDONLY)) {
        FatFile file;
        while (num_dir_entries < 99 && file.openNext(&dir, O_RDONLY)) {
            char filename[256];
            file.getName(filename, sizeof(filename));
       //     Serial.println(filename);
            if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
                file.close();
                continue;
            }
            
            if (file.isHidden() || file.isSystem()) {
                file.close();
                continue;
            }
            
            dst->isDir = file.isDir();
            if (!dst->isDir && !is_valid_file(filename)) {
                file.close();
                continue;
            }
            
            // Usa lo stesso nome per entrambi i campi
            strncpy(dst->long_filename, filename, 31);
            dst->long_filename[31] = 0;
            strncpy(dst->filename, filename, 24);
            dst->filename[24] = 0;
            
            dst->full_path[0] = 0;
            dst++;
            num_dir_entries++;
            
            file.close();
        }
        dir.close();
        
        qsort((DIR_ENTRY *)&files[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
        ret = 1;
    }
    else {
        Serial.println("Can't read directory");
    }
    
    return ret;
}

/* load file in  ROM */

/////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////* load file in  ROM */////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

void load_file(char *filename) {
    Serial.print("load file: ");
    Serial.println(filename);

    if (!(file.open(filename))) {
        Serial.print("Can't open file ");
        Serial.println(filename);
        return;
    }

    // 🔥 AZZERA IL BUFFER PRIMA DI LEGGERE
    memset(game, 0, BINLENGTH);
    
    int n = file.read(game, BINLENGTH);
    Serial.print("Read: ");
    Serial.print(n);
    Serial.println(" bytes");
    file.close();
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
  //Serial.print("cmd: ");  Serial.println(tipo);	
/////////////////// TIPO 1 /////////////////// 
  	if (tipo==1) {
		ret = read_directory(curPath);
    if (!(ret)) {
        curPath[0]='/';
        curPath[1]=0;
        read_directory(curPath);
        //Serial.println("Dir missed, return root");
        //error(1);
    }
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
//                     SETUP
////////////////////////////////////////////////////////////////////////////////////

void setup() {

  //
   
  gpio_init_mask(ALL_GPIO_MASK);
  pinMode(DSRAM_PIN,OUTPUT);
  digitalWrite(DSRAM_PIN,HIGH);

 // Initialize the bus state variables
  
  
  pinMode(CEROM2_PIN,INPUT);
 
 bool carton = false;
  int t = 100;
  
  while (gpio_get(CEROM2_PIN) == 0 && to_ms_since_boot(get_absolute_time()) < 2000) {
    if (to_ms_since_boot(get_absolute_time()) > t) {
      t += 100;
    }
    sleep_ms(1);
  }
  
 if (gpio_get(CEROM2_PIN) == 1) {
    carton = true;  
 }
  
   vreg_set_voltage(VREG_VOLTAGE_1_15); // set to 1_15 or 1_20 if you experience some glitches
  sleep_ms(10);
  set_sys_clock_khz(250000, true); // settled in compiler IDE as 250mhz overclocked

  Serial.begin(115200);
  //while(!Serial);

  
  Serial.println("P-ON your SEGA!");
  

  // Inizializza flash
  flash.begin();
  
  // Configura USB MSC
  usb_msc.setID("SD-1000", "External Flash", "1.0");
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  usb_msc.setCapacity(flash.size() / 512, 512);
  usb_msc.setUnitReady(true);
  usb_msc.begin();
  
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    sleep_ms(10);
    TinyUSBDevice.attach();
    sleep_ms(10);
  }
  
  // Monta filesystem
  fs_formatted = fatfs.begin(&flash);
  
  if (!carton) {
    // Modalità PC
    usb_msc.setUnitReady(true);
    while (1) {
      gpio_put(25,1);
      delay(100);
      gpio_put(25,0);
      delay(500);
    }
  }
  
  // === MODALITÀ CONSOLE ===
  usb_msc.setUnitReady(false);
  
  memset(ROM,0,BINLENGTH);
  
    loadROM();
   
}
////////////////////////////////////////////////////////////////////////////////////
//                     MAIN LOOP
////////////////////////////////////////////////////////////////////////////////////    
  
void loop() 

{
  int i=0;
  int cmd=0; // start with read files
  // Initialize GPIO pins
   curPath[0]='/';
   curPath[1]=0;
 
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
  //printLog();
  
  //Serial.println("");
  i=0;
 }

////////////////////////////////////////////////////////////////////////////////////
//                     MAIN LOOP
////////////////////////////////////////////////////////////////////////////////////    
  
void setup1()
{
   
}
