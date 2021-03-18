#include "Arduino.h"
#include "module1.h"
#include "gsm.h"

#include <GSM.h>
#include <DHT.h>
#include <EEPROM.h>

int error = 0; // TODO: make error handling

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

long last_T_update;

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
  handle_gsm();

  if((millis()-last_T_update) >= params.T_pollrate) {
    update_inputs();
    compute();
    flush_outputs();
  }
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
    if (gsm.begin("") == GSM_READY) {
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

void gsm_send_sms(char * body, char * to) {
  Serial.print("GSM: SMS sending to ");
  Serial.println(to);
  
  gsm_sms.beginSMS(to);
  gsm_sms.print(body);
  gsm_sms.endSMS();

  Serial.println("GSM: sent");
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
  last_T_update = 0;
}

/* from module1.h ver1 (not prod):

#define DHT_PIN 2
#define DHT_TYPE DHT22
#define RELAY1_PIN 3
#define RELAY2_PIN 4
#define HEAT_REQ_PIN 5
#define TH_RELAY_PIN 6

from gsmshield_pins.h:
#define GSM_TX 7
#define GSM_RX 8
#define GSM_PWR 9
 */
void hw_init() {
  flush_outputs();

  digitalWrite(GSM_PWR, GSM_POWER_ON);
  
  pinMode(GSM_RX, INPUT);
  pinMode(GSM_TX, OUTPUT);
  pinMode(GSM_PWR, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(TH_RELAY_PIN, OUTPUT);
  pinMode(HEAT_REQ_PIN, OUTPUT);

  dht.begin();
}

// Temperature, Humidity
void update_inputs() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  inputs.humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  inputs.temperature = dht.readTemperature();
}

// r1, r2, r_th, r_heatreq
void flush_outputs() {
  digitalWrite(RELAY1_PIN, outputs.relay1);
  digitalWrite(RELAY2_PIN, outputs.relay2);
  digitalWrite(TH_RELAY_PIN, outputs.relay_th);
  digitalWrite(HEAT_REQ_PIN, outputs.relay_heatreq);
}

/*
 * GSM command handling
 */

const char * admins[] = {
  "+380504181364",
  ""
};

typedef void (*CMDHandler)(void);
struct CMD {
  const char * cmd;
  CMDHandler handler;
};

// HANDLERS
void handle_get_T() {
  update_inputs();
  char reply[30] = "";
  sprintf(reply, "T:%.2f; H:%.2f;\0", inputs.temperature, inputs.humidity);
  gsm_send_sms(reply, sms.sender);
}

void handle_get_outputs() {
  char reply[30] = "";
  sprintf(reply, "opmode:%d; R1:%d; R2:%d; RTH:%d; HEATREQ:%d (override:%d);\0", 
    params.operation_mode, outputs.relay1, outputs.relay2, outputs.relay_th, outputs.relay_heatreq, params.heatreq_override);
  gsm_send_sms(reply, sms.sender);
}

void handle_get_params() {
  char reply[30] = "";
  sprintf(reply, "Tset:%.2f; Tpoll:%d; opmode:%d; override:%d;\0", 
    params.T_setting, params.T_pollrate, params.operation_mode, params.heatreq_override);
  gsm_send_sms(reply, sms.sender);
}

// true if changed. false if not in sms body.
// Field must be '='-terminated!
// BOOL
bool parse_set(const char * field, bool &res) {
  int taglen = strlen(field);
  char * pch = strstr(sms.body, field);
  if (!pch) return false; // substring not found. else:
  
  if (pch[taglen] == '1') res=true;
  else if (pch[taglen] == '0') res=false;
  else return false; // invalid syntax
  
  return true;
}
// TODO: add float and int parsers

// opmode, override, heatreq, R1/2
void handle_set() {
  int n_changed = 0;
  bool newval;
  if (parse_set("comfort=", newval)) { params.operation_mode = newval ? COMFY : ECO; n_changed++; }
  if (parse_set("override=", newval)) { params.heatreq_override = newval; n_changed++; }
  if (parse_set("heatreq=", newval)) {
    params.heatreq_override = true;
    outputs.relay_heatreq = newval;
    n_changed++;
  }
  if (parse_set("R1=", newval)) { outputs.relay1 = newval; n_changed++; }
  if (parse_set("R2=", newval)) { outputs.relay2 = newval; n_changed++; }

  // TODO: add float and int params
  
  char reply[20] = "";
  sprintf(reply, "%d settings saved\0", n_changed);
  gsm_send_sms(reply, sms.sender);
}

void handle_test() {
  outputs.relay1 = !outputs.relay1;
  
  char reply[20] = "";
  sprintf(reply, "relay 1 to %d\0", outputs.relay1);
  gsm_send_sms(reply, sms.sender);
}
// end HANDLERS

CMD commands[] = {
  { "test;", handle_test },
  { "get T;", handle_get_T },
  { "get outputs;", handle_get_outputs },
  { "get params;", handle_get_params },
  { "set;", handle_set },
  { "", 0 }
};
#define maxprefix 11
#define CMD_SUFFIX (':')

void handle_gsm() {
  if (gsm_get_sms()) { // sms received
    char * pch = 0;
    CMD * cmd = commands;
    while (cmd->handler) {
      pch = strstr(sms.body, cmd->cmd);
      if (!pch) {
        cmd++;
        continue;
      }
      
      // prefix successfully parsed:
      Serial.print("CMD: parsed:");
      Serial.println(cmd->cmd);
      cmd->handler();
    }
    
    // nothing passed -> invalid command
    if (!cmd->handler) {
      Serial.println("CMD: invalid command");
      char * reply = "invalid command";
      gsm_send_sms(reply, sms.sender);
      return;
    }

    update_inputs();
    compute();
    flush_outputs();
  }
}

/*
 * COMPUTE
 */

void compute() {
  if (!params.heatreq_override) {
    outputs.relay_heatreq = (inputs.temperature < params.T_setting); // placeholder algorithm
  } // otherwise just do not compute, leave relay_heatreq unchanged
}
