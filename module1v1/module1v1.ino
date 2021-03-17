#include "Arduino.h"
#include "module1.h"
#include "gsm.h"

#include <GSM.h>
#include <DHT.h>
#include <EEPROM.h>

int error = 0; // TODO: make error handling

#define PINNUMBER ""
#define SMSBUFFER 64
GSM gsm;
GSM_SMS gsm_sms;
struct {
  char sender[20];
  char body[SMSBUFFER];
} sms;

void gsm_init(); // +
int gsm_get_sms(); // +

RParams params; 
RIn inputs;
ROut outputs;

ROM rom; // rom.crc = CRC_OK | CRC_FAIL
Repo repo = {&params, &inputs, &outputs};

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(9600);
  load_rom();
  Serial.print("{ ID:");
  Serial.print(rom.id);
  Serial.print(", VER:");
  Serial.print(rom.ver);
  Serial.println(" }");

  gsm_init();
  
  repo_init();
  hw_init();
}

void loop() {
  handle_gms();
  
// TODO
}

/*
 * DEFINITIONS
 */

// GSM
//

void gsm_init() {
  bool isConnected = false;

  Serial.println("GSM: Connecting");
  while (!isConnected) {
    if (gsmAccess.begin(PINNUMBER) == GSM_READY) {
      isConnected = true;
    } else {
      Serial.println("GSM: Trying to connect");
      delay(1000);
    }
  }
  Serial.println("GSM: Connected!");
}

int gsm_get_sms() {
  // If there are any SMSs available()
  if (gsm_sms.available()) {
    Serial.print("GSM: SMS from ");

    // Get remote number
    gsm_sms.remoteNumber(sms.sender, 20);
    Serial.println(sms.sender);
    
    // An example of message disposal
    // Any messages starting with # should be discarded
    if (gsm_sms.peek() == '#') {
      Serial.println("Discarded SMS");
      gsm_sms.flush();
    }

    int i;
    // Read message bytes and print them
    for(i=0; i<SMSBUFFER; i++) {
      char c = gsm_sms.read();
      if (!c) {
        sms.body[i] = 0;
        break;
      }
      sms.body[i] = c;
      Serial.print(c);
    }
    sms.body[SMSBUFFER-1] = 0;
    
    Serial.println("\nEND OF MESSAGE");
    // Delete message from modem memory
    gsm_sms.flush();
    Serial.println("MESSAGE DELETED");
    
    return i; // n of read bytes
  }

  return 0; //else
  delay(1000);
}



// ROM
//

unsigned long CRC(byte * data, size_t bytes) {
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = 0 ; index < bytes  ; ++index) {
    const byte b = data[index];
    crc = crc_table[(crc ^ b) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (b >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}

void save_rom() {
  rom.last_params = params;
  // ... save other data here ...

  // calculate crc
  rom.crc = 0;
  rom.crc = CRC((byte*)&rom, sizeof(ROM));
  EEPROM.put(0, rom);
  rom.crc = CRC_OK;
}

void load_rom() {
  EEPROM.get(0, rom);
  unsigned long rom_checksum = rom.crc;
  rom.crc = 0;
  
  // validate
  unsigned long crc = CRC_ROM(&rom);
  rom.crc = (crc == rom_checksum) ? CRC_OK : CRC_FAIL;
  if (rom.crc == CRC_FAIL) {
    // failed, use default rom
    rom = DEFAULT_ROM;
    // save default rom in ROM
    save_rom();
  }
}

// REPOSITORY
//

void repo_init() {
  params = rom.last_params;
  inputs = DEFAULT_RIn;
  outputs = DEFAULT_ROut;
}

/* from module1.h ver1:
#define BUS_PIN 2 - pinMode self-controlled
#define TXCTRL_PIN 5 // TODO: check
#define RELAY1_PIN 3
#define RELAY2_PIN 4
#define TH_RELAY_PIN 6
#define DHT_PIN 8 - pinMode self-controlled
#define DHT_TYPE DHT22
from gsmshield_pins.h:
#define GSM_TX 7
#define GSM_RX 8
#define GSM_PWR 9
 */
void hw_init() {
  digitalWrite(BUS_PIN, LOW);
  digitalWrite(TXCTRL_PIN, BUS_MODE_RX);
  digitalWrite(RELAY1_PIN, outputs.relay1);
  digitalWrite(RELAY2_PIN, outputs.relay2);
  digitalWrite(TH_RELAY_PIN, outputs.relay_th);
  digitalWrite(GSM_PWR, GSM_POWER_ON);
  
  pinMode(GSM_RX, INPUT);
  pinMode(BUS_PIN, INPUT);
  pinMode(GSM_TX, OUTPUT);
  pinMode(GSM_PWR, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(TH_RELAY_PIN, OUTPUT);

  
}

typedef void (*CMDHandler)(void)
struct CMD {
  const char * cmd;
  
}

void handle_gsm() {
  if (gsm_get_sms()) { // sms received
    
  }
  
}
