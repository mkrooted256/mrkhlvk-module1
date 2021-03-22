#ifndef GSMSHIELD_H
#define GSMSHIELD_H

#define GSM_TX 8
#define GSM_RX 10
#define GSM_PWR 9

#define GSM_POWER_ON LOW // TODO: check

#include "Arduino.h"
#include <SoftwareSerial.h>

#define GSM_TIMEOUT 5000
#define GSMOUT_BUFLEN 250
#define GSM_BUFLEN 250
#define SMS_BUFLEN 250

#define CR '\r'
#define NL '\n'
#define END (char(26))
#define ESC (char(27))

#define REC_UNREAD 0
#define REC_READ 1
#define REC_OTHER 3

struct RecSMS {
  int index;
  int read;
  char * number;
  char * date;
  char * text;
};

#define OK 0
#define ERR 1
#define SMS_REC_ERR 2

int gsm_error = OK; 

char gsmoutbuf[GSMOUT_BUFLEN]; // output buffer
char gsmbuf[GSM_BUFLEN]; // input and general-purpose buffer
char publicbuf[SMS_BUFLEN]; // public buffer. no unexpected changes, so others can use it

SoftwareSerial sim_serial(GSM_RX, GSM_TX);

bool gsmshield_init();
size_t _read_serial(char * buf, size_t n=GSM_BUFLEN);
void gsmshield_clear_sms();
void gsmshield_send_sms(char * number, char * text);
int gsmshield_receive_sms();

// blocking!
bool gsmshield_init() {
  sim_serial.begin(9600);
  sim_serial.setTimeout(GSM_TIMEOUT);
  bool success = false;
  while (!success) {
    Serial.println("GSM: Connecting");
    sim_serial.print("ATE0\r");
    sim_serial.print("AT\r");
    _read_serial(gsmbuf);
    if (strstr(gsmbuf, "OK")) success = true;
    Serial.println(gsmbuf);
    delay(1000);
  }
}

size_t _read_serial(char * buf, size_t n=GSM_BUFLEN){
  memset(buf, 0, n);
  return sim_serial.readBytes(buf, n);
}

void gsmshield_clear_sms() {
  sprintf(gsmoutbuf, "AT+CMGD=1,3\r");
  sim_serial.print(gsmbuf);
  sim_serial.setTimeout(1000);
  _read_serial(gsmbuf);
  sim_serial.setTimeout(GSM_TIMEOUT);
}

void gsmshield_send_sms(char * number, char * text) {
//  sim_serial.setTimeout(10000);
  sprintf(gsmoutbuf, "%c\rAT+CMGS=\"%s\",\r", ESC, number);
    Serial.print(">1>");
    Serial.print(gsmoutbuf);
    Serial.println("<1<");
  sim_serial.print(gsmoutbuf);
  _read_serial(gsmbuf);
    Serial.println(">>");
    Serial.println(gsmbuf);
    Serial.println("<<");
  sprintf(gsmoutbuf, "%s%c\r", text, END);
    Serial.print(">2>");
    Serial.print(gsmoutbuf);
    Serial.println("<2<");
  sim_serial.print(gsmoutbuf);
  _read_serial(gsmbuf);
    Serial.println(">>>");
    Serial.println(gsmbuf);
    Serial.println("<<<");
  if (strstr(gsmbuf, "+CMGS:")) {
    gsm_error = OK;
  } else {
    gsm_error = ERR;
    Serial.println("GSM: SMS send error >>>>");
    Serial.println(gsmbuf);
    Serial.println("<<<<");
  }
//  sim_serial.setTimeout(GSM_TIMEOUT);
}


int gsmshield_receive_sms(RecSMS * smsbuf) {
  int smsbuf_n = 0;
  
  sim_serial.print(ESC);
  sim_serial.print("AT+CMGL=\"REC UNREAD\"\r");
  _read_serial(gsmbuf);

  Serial.println(">>>>");
  Serial.println(gsmbuf);
  Serial.println("<<<<");
  
  // +CMGL: index,"read","number","phonebook entry","date"\r\n
  // sms body\r\n
  // OK\r
  
  strcpy(publicbuf, gsmbuf);
  
  char * pch = strchr(publicbuf, '+');
  
  while (pch) {
    if (strncmp(pch, "+CMS ERR", 8) == 0) {
      // TODO: cms error
      break;
    }
    // sms record. skip "CMGL: "
    pch += 7;
    // 1. Index
    pch = strtok(pch, ","); 
    smsbuf[smsbuf_n].index = atoi(pch);
//    Serial.println(pch);
    // 2. Read (only unread because AT was such)
    pch = strtok(NULL, ","); 
//    Serial.println(pch);
    smsbuf[smsbuf_n].read = REC_UNREAD;
    // 3. Number
    pch = strtok(NULL, ","); 
//    Serial.println(pch);
    smsbuf[smsbuf_n].number = pch+1; // skip first \"
    // 4. Phonebook entry, ignore for now
    pch = strtok(NULL, ",");
//    Serial.println(pch);
    *(pch-2) = 0; // properly end Number, replace last \" by \0
    // 5. Date
    pch = strtok(NULL, "\r\n");
//    Serial.println(pch);
    smsbuf[smsbuf_n].date = pch+1; // skip first \"
    // 6. end Date and save SMS Text
    pch = strtok(NULL, "\r\n");
//    Serial.println(pch);
    *(pch-3) = 0;
    smsbuf[smsbuf_n].text = pch;
    smsbuf_n++;

    pch = strchr(pch, 0);
    while (!pch[0]) pch++; // find first not-0 char
//    Serial.println(pch);
    pch = strchr(pch, '+');
//    Serial.println(pch);
  }

  // TODO: look for "OK"
  
//  gsm_clear_sms();
//  delay(20);

  return smsbuf_n;
}

#endif // GSMSHIELD_H
