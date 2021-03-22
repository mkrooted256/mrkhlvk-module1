#include "Arduino.h"
#include "module1.h"
#include "gsmshield.h"

#include <DHT.h>
#include <EEPROM.h>

int error = 0; // TODO: make error handling

void gsm_init(); 
int gsm_get_sms(); 

RecSMS smsbuf[5];
char replybuf[140];

RParams params; 
RIn inputs;
ROut outputs;

long last_T_update = 0;

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

//  Serial.println(rom.last_params.T_pollrate);

  gsm_init();
  repo_init();
  hw_init();
  
//  Serial.println(rom.last_params.T_pollrate);
  
  Serial.println("Updating");
  update_inputs();
  compute();
  flush_outputs();
}

bool MANUAL = false;

void loop() {
  if (MANUAL) {
    if(sim_serial.available())
    {
      Serial.write(sim_serial.read());
    }
  
    if(Serial.available())
    {
      char c = Serial.read();
      if (c=='$') c = 26;
      else if (c=='^') c = 27;
      else if (c=='\\') {MANUAL = false; return;}
      sim_serial.write(c);
      //Serial.write(c);
    }
  } else {
    Serial.print("scmd: ");
    delay(1000);
    if(Serial.available() and Serial.peek()=='\\') { Serial.read(); MANUAL = true; return; }
    
    handle_gsm();
  
    if((millis()-last_T_update) >= params.T_pollrate) {
      Serial.print("Updating cause ");
      Serial.println(millis()-last_T_update);
      update_inputs();
      compute();
      flush_outputs();
      last_T_update = millis();
    }
  }
}

/*
 * DEFINITIONS
 */

// GSM
//

void gsm_init() {
  gsmshield_init();
}

int gsm_get_sms() {
  Serial.println("GSM: checking unread SMSs");
  int received = gsmshield_receive_sms(smsbuf);
  if (received) {
    gsmshield_clear_sms();
  }
  delay(2000);
  return received;
}

void gsm_send_sms(char * body, char * to) {
  Serial.print("GSM: SMS sending to ");
  Serial.println(to);
  
  gsmshield_send_sms(to, body);

  Serial.println("GSM: sent");
  delay(2000);
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

void save_to_rom() {
  rom.last_params = params;
  // ... etc ...
}

void save_rom() {
  Serial.println("Saving ROM");

  // calculate crc
  rom.crc = 0;
  rom.crc = CRC((byte*)&rom, sizeof(ROM));
  Serial.print("CRC: ");
  Serial.println(rom.crc);
  EEPROM.put(0, rom);
  rom.crc = CRC_OK;
}

void load_rom() {
  Serial.println("Loading ROM");
  EEPROM.get(0, rom);
  unsigned long rom_checksum = rom.crc;
  rom.crc = 0;
  
//  Serial.println(rom.last_params.T_pollrate);

//  Serial.println("ROM: {");
//  Serial.println("Last Params: {");
//  Serial.print(rom.last_params.T_setting);
//  Serial.print(" ");
//  Serial.print(rom.last_params.T_pollrate);
//  Serial.print(" ");
//  Serial.print(rom.last_params.operation_mode);
//  Serial.print(" ");
//  Serial.print(rom.last_params.heatreq_override);
//  Serial.println(" }}");
  
  // validate
  if (rom.last_params.T_pollrate == 0 or rom.last_params.T_setting == 0) {
    Serial.println("zeroes in rom. restoring defaults");
    rom = DEFAULT_ROM;
//    Serial.println(rom.last_params.T_pollrate);
    save_rom();
//    Serial.println(rom.last_params.T_pollrate);
    return;
  }
  
  unsigned long crc = CRC_ROM(&rom);
  rom.crc = (crc == rom_checksum) ? CRC_OK : CRC_FAIL;
  if (rom.crc == CRC_FAIL) {
    Serial.println("CRC failed, restoring defaults");
    // failed, use default rom
    rom = DEFAULT_ROM;
    // save default rom in ROM
    save_rom();
    return;
  }
}

// REPOSITORY
//

void repo_init() {
  params = rom.last_params;
  inputs = DEFAULT_RIn;
  outputs = DEFAULT_ROut;
  last_T_update = 0;

  
  Serial.println("Repo init. ");
  Serial.println("Params: {");
  Serial.print(params.T_setting);
  Serial.print(" ");
  Serial.print(params.T_pollrate);
  Serial.print(" ");
  Serial.print(params.operation_mode);
  Serial.print(" ");
  Serial.print(params.heatreq_override);
  Serial.println(" }");
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

//  digitalWrite(GSM_PWR, GSM_POWER_ON);
  
//  pinMode(GSM_RX, INPUT);
//  pinMode(GSM_TX, OUTPUT);
//  pinMode(GSM_PWR, OUTPUT);
//  pinMode(RELAY1_PIN, OUTPUT);
//  pinMode(RELAY2_PIN, OUTPUT);
//  pinMode(TH_RELAY_PIN, OUTPUT);
//  pinMode(HEAT_REQ_PIN, OUTPUT);

//  dht.begin();
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
//  digitalWrite(RELAY1_PIN, outputs.relay1);
//  digitalWrite(RELAY2_PIN, outputs.relay2);
//  digitalWrite(TH_RELAY_PIN, outputs.relay_th);
//  digitalWrite(HEAT_REQ_PIN, outputs.relay_heatreq);
}

/*
 * GSM command handling
 */

const char * admins[] = {
  "+380504181364",
  "+380672206823",
  ""
};

typedef void (*CMDHandler)(RecSMS*);
struct CMD {
  const char * cmd;
  CMDHandler handler;
};

// HANDLERS
void handle_get_T(RecSMS * sms) {
  update_inputs();
  PrFloat f1 = printfloat(inputs.temperature), f2 = printfloat(inputs.humidity);
  sprintf(replybuf, "T:%d.%d; H:%d.%d;", f1.i, f1.f, f2.i, f2.f );
  gsm_send_sms(replybuf, sms->number);
}

void handle_get_outputs(RecSMS * sms) {
  sprintf(replybuf, "opmode:%d; R1:%d; R2:%d; RTH:%d; HEATREQ:%d (override:%d);", 
    params.operation_mode, outputs.relay1, outputs.relay2, outputs.relay_th, outputs.relay_heatreq, params.heatreq_override);
  gsm_send_sms(replybuf, sms->number);
}

void handle_get_params(RecSMS * sms) {
  Serial.print("get params: ");
  Serial.print(params.T_setting);
  sprintf(replybuf, "Tset:%d.%d; Tpoll:%ul; opmode:%d; override:%d;\0", 
    params.T_setting, params.T_setting*100 - int(params.T_setting)*100, params.T_pollrate, params.operation_mode, params.heatreq_override);
  gsm_send_sms(replybuf, sms->number);
}

// true if changed. false if not in sms body.
// Field must be '='-terminated!
// BOOL
bool parse_set(RecSMS * sms, const char * field, bool &res) {
  int taglen = strlen(field);
  char * pch = strstr(sms->text, field);
  if (!pch) return false; // substring not found. else:
  
  if (pch[taglen] == '1') res=true;
  else if (pch[taglen] == '0') res=false;
  else return false; // invalid syntax
  
  return true;
}
// TODO: add float and int parsers

// opmode, override, heatreq, R1/2
void handle_set(RecSMS * sms) {
  int n_changed = 0;
  bool newval;
  if (parse_set(sms, "comfort=", newval)) { params.operation_mode = newval ? COMFY : ECO; n_changed++; }
  if (parse_set(sms, "override=", newval)) { params.heatreq_override = newval; n_changed++; }
  if (parse_set(sms, "heatreq=", newval)) {
    params.heatreq_override = true;
    outputs.relay_heatreq = newval;
    n_changed++;
  }
  if (parse_set(sms, "R1=", newval)) { outputs.relay1 = newval; n_changed++; }
  if (parse_set(sms, "R2=", newval)) { outputs.relay2 = newval; n_changed++; }

  // TODO: add float and int params

  Serial.print(n_changed);
  Serial.println(" settings saved");
  sprintf(replybuf, "%d settings saved\0", n_changed);
  gsm_send_sms(replybuf, sms->number);
}

void handle_test(RecSMS * sms) {
  outputs.relay1 = !outputs.relay1;
  
  sprintf(replybuf, "relay 1 to %d\0", outputs.relay1);
  gsm_send_sms(replybuf, sms->number);
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
#define CMD_SUFFIX (';')

void handle_gsm() {
  int received = gsm_get_sms();
  for (int i=0; i<received; i++) {
    RecSMS * sms = smsbuf+i;
    Serial.print("Parsing sms");
    Serial.print(i);
    Serial.print(";");
    Serial.println(sms->index);
    Serial.println(sms->number);
    Serial.println(sms->text);
    Serial.print(sms->date);
    Serial.println(";;");
    
    char * pch = 0;
    CMD * cmd = commands;
    while (cmd->handler) {
      pch = strstr(sms->text, cmd->cmd);
      if (!pch) {
        cmd++;
        continue;
      }
      
      // prefix successfully parsed:
      Serial.print("CMD: parsed:");
      Serial.println(cmd->cmd);
      cmd->handler(sms);
      break;
    }
    
    // nothing passed -> invalid command
    if (!cmd->handler) {
      Serial.println("CMD: invalid command");
      char * reply = "invalid command";
      gsm_send_sms(reply, sms->number);
      continue;
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
