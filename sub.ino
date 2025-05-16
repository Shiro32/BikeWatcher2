#include <stdlib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// for OTA
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <uTimerLib.h>

#include "sub.h"

WiFiMulti wifi_multi;
WiFiClient wifi_client;

volatile uint32_t rightBtnTick, leftBtnTick;

volatile BTN_STATUS leftBtnStatus=BTN_NOTHING, rightBtnStatus=BTN_NOTHING;

// 震動検知通知用（割り込みのvolatile変数）
volatile bool gVibrationDetected=false;

volatile uint32_t gSystemTick_s=0;

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

// -------------------------------------------------------------------------
// TW2525A関係（振動通知）
void AttachTW2525Interrupt( void ) {
	gVibrationDetected = false;

	pinMode( GPIO_TW2525_D1, INPUT_PULLDOWN );
	pinMode( GPIO_TW2525_D2, INPUT_PULLDOWN );
	pinMode( GPIO_TW2525_D4, INPUT_PULLDOWN );

	attachInterrupt( GPIO_TW2525_D1, TW2525Handler, CHANGE );
	attachInterrupt( GPIO_TW2525_D2, TW2525Handler, CHANGE );
	attachInterrupt( GPIO_TW2525_D4, TW2525Handler, CHANGE );
}

void DetachTW2525Interrupt( void ) {
	detachInterrupt( GPIO_TW2525_D1 );
	detachInterrupt( GPIO_TW2525_D2 );
	detachInterrupt( GPIO_TW2525_D4 );
}

// -------------------------------------------------------------------------
// 揺れを検出した際の処理ハンドラ（割り込みで呼ばれる）
// 割り込み処理はさっさと早く終わらせたいので、フラグだけ立てて終了
// 高速処理が必要とかで、内部RAMに置くための特別の宣言が必要
void IRAM_ATTR TW2525Handler( void ) {
	gVibrationDetected = 0;

	if( digitalRead(GPIO_TW2525_D1)==LOW ) gVibrationDetected += 1;
	if( digitalRead(GPIO_TW2525_D2)==LOW ) gVibrationDetected += 2;
	if( digitalRead(GPIO_TW2525_D4)==LOW ) gVibrationDetected += 4;

	if( gVibrationDetected==0 ) gVibrationDetected = 9;

	//gVibrationDetected = true;
}

// -------------------------------------------------------------------------
// 通信方式選択
// 起動直後はココの選択で止まっているはずなので、OTAもやっておく
void SelectCommMode( void ) {
	// 画面に何か出す
	oled.clear();
	oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, 1, "Select COMM MODE");
	oled.print( OLED_WIDTH/2, 20, ALIGN_CENTER,1, "L:DIRECT / R:WIFI" );
	oled.flush();

	// ボタンが押されるまで待つ
	while( true ) {
		// OTA処理
		ArduinoOTA.handle();

		if( leftBtnStatus==BTN_1CLICK ) {
			gCommMode = DIRECT_MODE;
			break;
		}
		if( rightBtnStatus==BTN_1CLICK ) {
			gCommMode = WIFI_MODE;
			break;
		}
	}

	oled.print( OLED_WIDTH/2, 30, ALIGN_CENTER, 2, gCommMode==DIRECT_MODE?"DIRECT":"WIFI" );
	oled.flush();

	delay(2000);
	oled.clear();
	oled.flush();

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
	// WiFi接続処理
	wifi_multi.addAP( SSID1, PSWD1 );
	wifi_multi.addAP( SSID2, PSWD2 );
	wifi_multi.addAP( SSID3, PSWD3 );
	wifi_multi.addAP( SSID4, PSWD4 );
}

void DisconnectWiFi( void ) {
	// WiWiを切断し、電源まで切っちゃう
	WiFi.disconnect( true );
}

void ConnectWiFi( void ) {
	String ssid, name, ip;

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
// １秒単位のシステムTICKをカウントアップする
// タイマーハンドラーで１秒単位に実行してもらっている
// 2025/5/15 変更
// setInterval_usを使って読んでいたが、キーの割り込み(GPIO)と競合する模様
// 割り込み中に割り込みが起きるとハングする
// 避けられそうにないので、通常のメインルーチンからの呼び出しに変更する

void CountSystemTickSec( void ) {
	gSystemTick_s++;
}
uint32_t SystemTickSec( void ) {
	return( gSystemTick_s );
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
}

