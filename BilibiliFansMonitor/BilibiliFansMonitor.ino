/*

       Bilibili Fans Clock   by xianfei
          version 1.0
          licensed under the GNU General Public License v3.0 (see https://www.gnu.org/licenses/gpl-3.0.html)

       Github: xianfei  https://github.com/xianfei/
       Bilibili: xianfei  https://space.bilibili.com/9872607

       Default hardward is Arduino Ethernet Shield (W5100 Ethernet Controller @ 10/100Mbps Adapter)
       (see more on https://www.arduino.cc/en/Main/ArduinoEthernetShieldV1 )

*/

#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"


// #define ENABLE_SERIAL  // serial output for debug

long bilibiliID = 9872607; // b站UID 默认值为xianfei的UID
int timeZone = +8; // 时区设定，默认北京所在的+8区
int timeOffset = -1; // 时钟偏移量 用于修正网络授时延时等
int refreshRate = 10; // 更新间隔秒数 （算法是秒数对此数字取模等于零时刷新

LiquidCrystal_I2C lcd(0x20, 16, 2); // 显示屏初始化
static long biliFans = -1;
static int h, m, s = 9, y, mon, day, xq; // 系统时间 时 分 秒 年 月 日 星期
unsigned long oldmillis = 0; //计时器

void setup() {
  lcd.init();
  lcd.backlight();
#if defined(ENABLE_SERIAL)
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
#endif
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  // Enter a MAC address for your controller below.
  byte mac[] = { 0x28, 0xAD, 0xBE, 0xEF, 0xB9, 0xED };
  if (!Ethernet.begin(mac)) { // try to congifure using IP address instead of DHCP:
    IPAddress ip(192, 168, 2, 222); // Set the static IP address to use if the DHCP fails to assign
#if defined(ENABLE_SERIAL)
    Serial.println("Failed to configure Ethernet using DHCP");
#endif
    Ethernet.begin(mac, ip);
  }
  lcd.setCursor(0, 0);
  lcd.print("IP:");
  lcd.print(Ethernet.localIP());
  lcd.setCursor(0, 1);
  lcd.print("UID:");
  lcd.print(bilibiliID);
#if defined(ENABLE_SERIAL)
  Serial.println(Ethernet.localIP());
#endif
  delay(2000);
  lcd.clear();
}

void loop() {
  unsigned long millis_ = millis();
  if (millis_ - oldmillis >= 1000) {
    oldmillis = millis_;
    output();
    if (s % refreshRate == 0) {
      long num;
      String httpContent = getHttp(String("http://api.bilibili.com/x/relation/stat?vmid=") + bilibiliID);
      if ((num = getBilibiliFollowerNumber(httpContent)) != -1) biliFans = num;
      syncTimeByHttpHead(httpContent);
#if defined(ENABLE_SERIAL)
      Serial.print(httpContent);
#endif
    }
  }
}

String getHttp(const String& addr) {
  EthernetClient client;
  char host[20], getAddr[50];
  if (!sscanf(addr.c_str(), "htt%*[^:]://%[^/]/%s", host, getAddr)) {
#if defined(ENABLE_SERIAL)
    Serial.println("Error HTTP Address");
#endif
    return String("error");
  }
  if (client.connect(host, 80)) {
    // Make a HTTP request:
    const auto request = String ("GET /") + getAddr + " HTTP/1.1\n" + "Host: " + host + "\nConnection: close\n";
    client.println(request);
  } else Serial.println("connection failed"); // if you didn't get a connection to the server:
  String resultStr;
  // wait for socket available
  while (!client.available());
  // ignore some unused content, only save "Date:" and json content
  char c = 0;
  while (client.available() && c != 'C') resultStr += c = (char)client.read();
  while (client.available() && c != '{') c = (char)client.read();
  while (client.available()) resultStr += (char)client.read();
  client.stop();
  return resultStr;
}

long getBilibiliFollowerNumber(const String& httpContent) {
  long followerNum = -1;
  sscanf(httpContent.substring(httpContent.indexOf("follower")).c_str(), "follower\":%ld%*s", &followerNum);
#if defined(ENABLE_SERIAL)
  Serial.print("Fans Got: ");
  Serial.println(followerNum);
#endif
  return followerNum;
}

void syncTimeByHttpHead(String& httpContent) {
  char date[4];
  if (!sscanf(httpContent.c_str(), "%*[^D]Date: %*[^,], %d %4s %d %d:%d:%d GMT%*s", &day, date, &y, &h, &m, &s)) {
#if defined(ENABLE_SERIAL)
    Serial.println("sync time error");
#endif
    return;
  }
  switch (date[0])
  {
    case 'J':
      if ( date[1] == 'a' ) mon = 1;
      else if ( date[2] == 'n' ) mon = 6;
      else mon = 7;
      break;
    case 'F':
      mon = 2;
      break;
    case 'A':
      mon = date[1] == 'p' ? 4 : 8;
      break;
    case 'M':
      mon = date[2] == 'r' ? 3 : 5;
      break;
    case 'S':
      mon = 9;
      break;
    case 'O':
      mon = 10;
      break;
    case 'N':
      mon = 11;
      break;
    case 'D':
      mon = 12;
      break;
  }
  h += timeZone;
  s += timeOffset;
  increaseTime();
}

void increaseTime() {
  s++;
  if (s > 59) {
    s %= 60;
    m++;
  }
  if (m > 59) {
    m %= 60;
    h++;
  }
  if (h > 23) {
    h %= 24;
    day++;
    byte daymax[13] = {100, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y / 100 != 0 && y % 4 == 0 || y / 400 == 0) daymax[2] = 29;
    else daymax[2] = 28;
    if (day > daymax[mon]) {
      day = 1;
      mon++;
      if (mon > 12) {
        mon = 1;
        y++;
      }
    }
  }
}

void output() {
  increaseTime();
  // output fans
  lcd.setCursor((10 - (int)log10(biliFans)) / 2, 1); // try to make center the text
  lcd.printf("Fans:%d",biliFans);
#if defined(ENABLE_SERIAL)
  Serial.println("Printed");
#endif
  // convert time
  lcd.setCursor(0, 0);
  lcd.printf(" %02d-%02d %02d:%02d:%02d", mon, day, h, m, s);
}
