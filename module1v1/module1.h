#ifndef MODULE1_H
#define MODULE1_H

#define MODULE1_H_VER 1

#include "Arduino.h"
#include "bus.h"
#include "gsmshield_pins.h"

#include <GSM.h>
#include <DHT.h>
#include <EEPROM.h>

#ifdef PROD

#define BUS_PIN 2
#define TXCTRL_PIN 5 // TODO: implement
#define RELAY1_PIN 3
#define RELAY2_PIN 4
#define TH_RELAY_PIN 6
#define DHT_PIN 8
#define DHT_TYPE DHT22

#else

#define DHT_PIN 2
#define DHT_TYPE DHT22
#define RELAY1_PIN 3
#define RELAY2_PIN 4
#define HEAT_REQ_PIN 5
#define TH_RELAY_PIN 6

#endif

// operation modes
#define ECO false  
#define COMFY true

/* 
 *  REPOSITORY
 *  params(rw), inputs(r), outputs(rw)
 */

struct RParams {
  float T_setting;
  long T_pollrate;
  bool operation_mode;
  bool heatreq_override;
};
struct RIn {
  float temperature;
  float humidity;
};
struct ROut {
  bool relay1;
  bool relay2;
  bool relay_th;
  bool relay_heatreq;
};

struct Repo {
  RParams *params;
  RIn *inputs;
  ROut *outputs;
};

// Defaults
const RParams DEFAULT_RParams = { /*T_setting*/18.0f, /*T_pollrate*/60000, /*operation_mode*/ECO, false};
const RIn DEFAULT_RIn = {NAN, NAN};
const ROut DEFAULT_ROut = { LOW, LOW, LOW, LOW };

/*
 *  ROM
 */

#define CRC_OK 0
#define CRC_FAIL 1

struct ROM {
  unsigned int id;
  unsigned int ver;
  RParams last_params;
  unsigned long crc;
};


// default ROM
const ROM DEFAULT_ROM = { 1, 1, DEFAULT_RParams, CRC_OK };

unsigned long CRC(byte * rom, size_t bytes);
#define CRC_ROM(p_rom) CRC((byte*)p_rom, sizeof(ROM))
void save_rom();
void load_rom();

/*
 *  GLOBALS
 */

// TODO: probably move here globals from module1v1.cpp
 

/* 
 *  FUNCTIONS
 */

// Load saved values from EEPROM, setup globals according to params
void repo_init(); // +

// pinmodes
void hw_init(); // +

// repo <-> io 
void update_inputs(); // +
void flush_outputs(); // +

// invoke commands; get/set repo data
void handle_gsm(); // +


// compute outputs
void compute(); // +


#endif //MODULE1_H
