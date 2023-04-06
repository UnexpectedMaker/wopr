//===========================================================================
//
// Set current time w/UTC offset and daylight saving time
// uses https://ipapi.co/<your-ip>/utc_offset/
//
// Copyright (c) 2023 BitBank Software, Inc.
// written by Larry Bank
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===========================================================================

#include <HTTPClient.h>
#include <time.h>

String GetSecondsUntilXmas()
{
  struct tm *xmas;
  time_t now = time(NULL);

  xmas = localtime(&now);

  //
  // are we on/after Christmas, go to next year
  //
  if ((xmas->tm_mon == 11) &&
      (xmas->tm_mday >= 25)) {
     xmas->tm_year++;
  }

  // tm_mon (0-11), tm_mday(1-31)
  xmas->tm_mon = 11;
  xmas->tm_mday = 25;
  xmas->tm_hour = 0;
  xmas->tm_min = 0;
  xmas->tm_sec = 0;

  // convert to seconds
  time_t xmasTime = mktime(xmas);
  
  unsigned long seconds = xmasTime - now;
  
  return String(seconds) + " Seconds until XMAS...";
} /* GetDaysUntilXmas() */

int GetTimeOffset(char *szIP)
{
  HTTPClient http;
  int httpCode = -1;
  char szTemp[256];

  //format -> https://ipapi.co/<your public ip>/utc_offset/
  sprintf(szTemp, "https://ipapi.co/%s/utc_offset/", szIP);
  http.begin(szTemp);
  httpCode = http.GET();  //send GET request
  if (httpCode != 200) {
    http.end();
    return -1;
  } else {
    const char *s;
    int i;
    String payload = http.getString();
    http.end();
    s = payload.c_str();
    // Get the raw HTTP response text (+HHMM)
    // and convert the time zone offset (HHMM) into seconds

    //  Serial.print("TZ offset ");
    //  Serial.println(s);
    i = ((s[1] - '0') * 10) + (s[2] - '0'); // hour
    i *= 60;
    i += ((s[3] - '0') * 10) + (s[4] - '0'); // minute
    if (s[0] == '-')
      i = -i; // negative offset
    return (i * 60); // return seconds
  } // if successfully connected
  return -1;
} /* GetTimeOffset() */
//
// Get our external IP from ipify.org
// Copy it into the given string variable
// in the form (a.b.c.d)
// Returns true for success
//
bool GetExternalIP(char *szIP)
{
  WiFiClient client;

  if (!client.connect("api.ipify.org", 80)) {
    Serial.println("api.ipify.org failed!");
    return false;
  }
  else {
    int timeout = millis() + 5000;
    client.print("GET /?format=json HTTP/1.1\r\nHost: api.ipify.org\r\n\r\n");
    while (client.available() == 0) {
      if (timeout - millis() < 0) {
        Serial.println("Client Timeout!");
        client.stop();
        return false;
      }
    }
    // Get the raw HTTP+JSON response text
    // and parse out just the IP address
    int i, j, size, offset = 0;
    char szTemp[256];
    while ((size = client.available()) > 0) {
      if (size + offset > 256) size = 256 - offset;
      size = client.read((uint8_t *)&szTemp[offset], size);
      offset += size;
    } // while data left to read

    // parse the IP address we want
    for (i = 0; i < offset; i++) {
      if (memcmp(&szTemp[i], "{\"ip\":\"", 7) == 0) {
        for (j = i + 7; j < offset && szTemp[j] != '\"'; j++) {
          szIP[j - (i + 7)] = szTemp[j];
        } // for j
        szIP[j - (i + 7)] = 0; // zero terminate it
        return true;
      } // if found start of IP
    } // for i
  } // if successfully connected
  return false;
} /* GetExternalIP() */
