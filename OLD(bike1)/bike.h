// bike.inoで使うヘッダファイル

#ifndef _BIKE_H_
#define _BIKE_H_

// WIFI接続関係（プレーンテキストはまずい気もするが・・・）
const char* SSID1	= "Shiro-iPhone";
const char* PSWD1	= "hogehoge";

const char* SSID2	= "C330";
const char* PSWD2	= "hogehoge";

const char* SSID3	= "Buffalo-G-F600";
const char* PSWD3	= "bvjsci3wpcf4g";

const char* LINE_TOKEN	= "zG7RzSGz0lore6SNGqF2VrFcVX1r2OcKGezJq7moAVQ";
const char* LINE_HOST	= "notify-api.line.me";

// GPIO関係（あんまり無いけど）
const int kVibrationPin = 5;	// 振動センサとつなぐGPIOピン
const int kPowerLedPin = 32;	// 電源確認用LEDのGPIOピン

// システム動作関係
const int kStartMonitorTimer_s = 30;	// （秒）監視開始タイマ（3分くらいがいい？）
const int kVibrationTimer_s = 2;		// どれくらい継続したら震動検知とするか（秒）


// 各種アイコン（PGOGMEM必要なのか？）
static const uint8_t PROGMEM
	kSmileBmp[]		={B00111100,B01000010,B10100101,B10000001,B10100101,B10011001,B01000010,B00111100 },
	kNeutralBmp[]	={B00111100,B01000010,B10100101,B10000001,B10111101,B10000001,B01000010,B00111100 },
	kFrownBmp[]		={B00111100,B01000010,B10100101,B10000001,B10011001,B10100101,B01000010,B00111100 },
	kBikkuriBmp[]	={B00011000,B00111100,B00111100,B00111100,B00011000,B00000000,B00011000,B00011000},
	kXBmp[]			={B10000001,B01000010,B00100100,B00011000,B00011000,B00100100,B01000010,B10000001},
	kWiFiBmp[]		={B00000000,B00111100,B01000010,B10000001,B00111100,B01000010,B00011000,B00011000},
	kStopBmp[]		={B00111100,B01000110,B10001111,B10011101,B10111001,B11110001,B01100010,B00111100},
	kWaitBmp[]		={B01111110,B01000010,B00100100,B00011000,B00011000,B00100100,B01000010,B01111110};


void onShakeHandler();

#endif

