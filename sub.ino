#include <stdlib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// for OTA
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <uTimerLib.h>
#include <Ambient.h>

#include "sub.h"

WiFiMulti wifi_multi;
WiFiClient wifi_client;
Ambient ambient;

volatile BTN_STATUS rightBtnStatus, leftBtnStatus;
uint32_t rightBtnTick, leftBtnTick;

// -------------------------------------------------------------------------
// プッシュボタン関係
void IRAM_ATTR onLeftBtnPressed() {
	// 押された時（Falling）
	if( digitalRead(GPIO_BTN_LEFT)==LOW && leftBtnStatus==BTN_NOTHING ) {
		leftBtnStatus = BTN_PRESSING;
		leftBtnTick = millis();

		// ロングプレス用のタイマ割り込み（1回のみ）
		TimerLib.setTimeout_us( onLeftBtnPressed, BTN_LONGPRESS_TIME_ms*1000 );
		return;
	}

	// ①クリックで離された時（rising）
	// ②プレスしてタイムアウト
	// ③タイムアウトのあとに離したとき（rising）
	TimerLib.clearTimer();	// まず、タイマを止める

	// ①②のパターン（③は何もしない）
	// 押し始めからの経過時間でclick/press判定	
	if( leftBtnStatus==BTN_PRESSING )
		leftBtnStatus = (millis()-leftBtnTick)<BTN_LONGPRESS_TIME_ms?
							BTN_1CLICK:BTN_LONGPRESS;
}

void IRAM_ATTR onRightBtnPressed() {
	// 押された時（Falling）
	if( digitalRead(GPIO_BTN_RIGHT)==LOW && rightBtnStatus==BTN_NOTHING ) {
		rightBtnStatus = BTN_PRESSING;
		rightBtnTick = millis();
		TimerLib.setTimeout_us( onRightBtnPressed, BTN_LONGPRESS_TIME_ms*1000 );
		return;
	}
	TimerLib.clearTimer();
	if( rightBtnStatus==BTN_PRESSING )
		rightBtnStatus = (millis()-rightBtnTick)<BTN_LONGPRESS_TIME_ms?
							BTN_1CLICK:BTN_LONGPRESS;
}

void SetupButtons( void ) {
	rightBtnStatus	= BTN_NOTHING;
	leftBtnStatus	= BTN_NOTHING;

	pinMode( GPIO_BTN_LEFT, INPUT_PULLUP );
	attachInterrupt( GPIO_BTN_LEFT, onLeftBtnPressed, CHANGE );

	pinMode( GPIO_BTN_RIGHT, INPUT_PULLUP );
	attachInterrupt( GPIO_BTN_RIGHT, onRightBtnPressed, CHANGE );
}

void SelectCommMode( void ) {
	// 画面に何か出す
	oled.clear();
	oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, "Select COMM MODE");
	oled.print( OLED_WIDTH/2, 20, ALIGN_CENTER, "L:DIRECT / R:WIFI" );
	oled.flush();

	// ボタンが押されるまで待つ
	while( true ) {
		if( leftBtnStatus==BTN_1CLICK ) {
			gCommMode = DIRECT_MODE;
			break;
		}
		if( rightBtnStatus==BTN_1CLICK ) {
			gCommMode = WIFI_MODE;
			break;
		}
	}

	leftBtnStatus  = BTN_NOTHING;
	rightBtnStatus = BTN_NOTHING;

}



// -------------------------------------------------------------------------
// LiPoバッテリ関係

// -------------------------------------------------------------------------
// SOCを取り込むADCピンの設定
void SetupBattSOC( void ) {
	pinMode( GPIO_BATT_SOC, INPUT );
}

// -------------------------------------------------------------------------
// SOCを拾う。ただ、電圧をfloatで返すだけ
float GetBattSoc( void ) {
	uint32_t vbat = 0;
	for( uint8_t i=0; i<8; i++ ) vbat += analogReadMilliVolts( GPIO_BATT_SOC );
	return( 2.0*vbat/8/1000 );
}


// -------------------------------------------------------------------------
// OLEDのデフォルトコンストラクタ
// 継承元のAdafruitのコンストラクタを同時に初期化する
OLED::OLED( uint8_t w, uint8_t h, TwoWire *twi, int8_t rst_pin ) 
	: Adafruit_SSD1306( w, h, twi, rst_pin )
{
}

void OLED::print( uint8_t x, uint8_t y, TEXT_ALIGN h_align, uint8_t size, const char *str, bool erase ) {
	char s[60];
	int16_t x1, y1;
	uint16_t w1, h1;

	setTextSize( size );

	// ほとんどの場合でバウンダリーが必要なので、やっちゃおう
	strncpy( s, "01234567890123456789012345678901234567890123456789", strlen(str) );
	s[strlen(str)]=0;
	getTextBounds( s, 0, 0, &x1, &y1, &w1, &h1 );

	// テキスト揃え処理
	switch( h_align ) {
	case ALIGN_LEFT:
		break;
	case ALIGN_CENTER:
		x -= w1/2;
		x1 -= w1/2;
		break;
	case ALIGN_RIGHT:
		x -= w1;
		x1 -= w1;
		break;
	}

	if( erase ) {
		getTextBounds( s, x, y, &x1, &y1, &w1, &h1 );
		fillRect( x1, y1, w1<=OLED_WIDTH?w1:OLED_WIDTH-1, h1, BLACK );
	}

	setCursor( x, y );
	setTextColor( WHITE );
	print( str );
}

void OLED::print( uint8_t x, uint8_t y, TEXT_ALIGN h_align, uint8_t size, const char *str ) {
	print( x, y, h_align, size, str, true );
}

void OLED::print( uint8_t x, uint8_t y, uint8_t size, const char *str, bool erase ) {
	print( x, y, ALIGN_LEFT, size, str, erase );
}

void OLED::print( uint8_t x, uint8_t y, uint8_t size, const char *str ) {
	print( x, y, ALIGN_LEFT, size, str, true );
}

void OLEDflush( void ) {
	oled.display();
}


void OLED::fill( void ) {
	fillScreen(WHITE);
}

void OLED::clear( void ) {
	fillScreen(BLACK);

}

void OLED::flush( void ) {
	display();
}


// -------------------------------------------------------------------------
// WiFiの接続状況をチェックする
void SetupWiFi( void ) {
	String ssid, name, ip;

	// WiFi接続処理
	wifi_multi.addAP( SSID1, PSWD1 );
	wifi_multi.addAP( SSID2, PSWD2 );
	wifi_multi.addAP( SSID3, PSWD3 );
	wifi_multi.addAP( SSID4, PSWD4 );

	// まず接続チェック（接続済みならreturn）
	if( WiFi.status() == WL_CONNECTED ) return;

	// for OTA
	WiFi.mode( WIFI_STA );

	// 接続処理開始
	Serial.println( "Now trying to connect to WiFi" );

	switch( wifi_multi.run()  ) {
	case WL_CONNECTED:
		Serial.println( "Connected." );
		ip = WiFi.localIP().toString();
		Serial.print( "SSID: " ); Serial.println( WiFi.SSID() );
		Serial.print( "IPv4: " ); Serial.println( ip );
		break;
	default:
		Serial.println( "NOT connected." );
	}

}

//------------------------------------------------------------------------------
// ESP32のOTA(On The Air設定)のための処理
void SetupOTA( void ) {
	ArduinoOTA
		.onStart([]() {
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
				type = "sketch";
			else // U_SPIFFS
				type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			Serial.println("Start updating " + type);
		})

		.onEnd([]() {
			Serial.println("\nEnd");
		})

		.onProgress([](unsigned int progress, unsigned int total) {
			Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
		})

		.onError([](ota_error_t error) {
			Serial.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
			else if (error == OTA_END_ERROR) Serial.println("End Failed");
		});
	ArduinoOTA.begin();
}

// -------------------------------------------------------------------------
// タイマー処理
// 呼び出し側でタイマ管理変数を用意して、指定時間に到達したかをチェックできる
bool WaitSec( uint32_t* prev, uint8_t timer_s ) {
	
	// 初めての呼び出し時には現在時刻を記憶
	if( *prev==0 ) *prev = millis();

	if( millis() - *prev >= timer_s*1000 ) {
		*prev = 0;
		return true;
	} else {
		return false;
	}
}

// -------------------------------------------------------------------------
// LINEへの通知発信（いつもの奴）
void SendLineNotify(char *str) {
	// HTTPSへアクセス（SSL通信）するためのライブラリ
	WiFiClientSecure client;

	// サーバー証明書の検証を行わずに接続する場合に必要
	client.setInsecure();
	Serial.println("LINE NOTIFICATION");
	
	//LineのAPIサーバにSSL接続（ポート443:https）
	if (!client.connect(LINE_HOST, 443)) {
		Serial.println("Connection failed");
		return;
	}
	Serial.println("Line Connected");

	// リクエスト送信
	String query = String("message=") + String(str);
	String request=
	  String("") +
		"POST /api/notify HTTP/1.1\r\n" +
		"Host: " + LINE_HOST + "\r\n" +
		"Authorization: Bearer " + LINE_TOKEN + "\r\n" +
		"Content-Length: " + String(query.length()) +	"\r\n" + 
		"Content-Type: application/x-www-form-urlencoded\r\n\r\n" +
		query + "\r\n";
	client.print(request);
 
	// 受信完了まで待機 
	while (client.connected()) {
		String line = client.readStringUntil('\n');
		if (line == "\r") break;
	}
	
	String line = client.readStringUntil('\n');
	Serial.println(line);