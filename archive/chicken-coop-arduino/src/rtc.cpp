#include <Wire.h>
#include "conf.h"
#include "RTClib.h"

RTC_DS1307 DS1307_RTC;

static bool RTC_FOUND = false;
char DOW[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setupRtc() {
  if (! DS1307_RTC.begin()) {
    Serial.println("RTC not found at address 0x68");
  } else {
    RTC_FOUND = true;
#if FORCE_TIME_RELOAD == 1
    // following line sets the RTC to the date & time this sketch was compiled
    // DS1307_RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    DS1307_RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // January 21, 2014 at 3am you would call:
    // DS1307_RTC.adjust(DateTime(2014, 1, 21, 3, 0, 0));
#else
    if (! DS1307_RTC.isrunning()) {
      Serial.println("RTC is not running. Setting compile time and starting..");
      // following line sets the RTC to the date & time this sketch was compiled
      DS1307_RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
#endif
  }
}

void loopRtc() {
  if (RTC_FOUND) {
    DateTime now = DS1307_RTC.now();
    Serial.print("Current time read from RTC: ");
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(DOW[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
  }
}

