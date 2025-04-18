/*
  ファイル：led.ino
  目的：adafruitのled関係処理

  履歴：
  2022/10/16 初版稼働（WiFiの動的切替、音声発信など）

  LEDの状態
  ①welcome的なアイコン
  ②wifi無しアイコン
  ③wifi接続SSID表示
  ④スタンバイ表示（バーグラフ）
  ⑤通常アイコン（blink）
  ⑥振動表示（お怒りアイコンblink）
  ⑦振動表示（スクロール）
*/

#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

bool blink_led_sw = true;
Adafruit_8x8matrix matrix88 = Adafruit_8x8matrix();
int system_timer_sec = 0;				// 1秒ごとにカウントアップ

// -------------------------------------------------------------------------
// InitMatrix88
void InitMatrix88( void ) {
	matrix88.begin( kMatrix88Adr );	// pass in the address
	matrix88.setTextSize( 1 );
	matrix88.setTextWrap( false );	// we dont want text to wrap so it scrolls nicely
	matrix88.setTextColor( LED_ON );
}

// -------------------------------------------------------------------------
// gauge88
// led matrixを8x8=64ドットのゲージとして使う
// now: 現在値
// max: 最大値
void IndicatorMatrix88(int now, int max ) {
	int x, y;

	matrix88.clear();
	matrix88.blinkRate(0);

	// 現在値を64に標準化
	now = now * 64 / max;

	for( y=0; y<8; y++ ) {
		if( now==0 ) break;

		for( x=0; x<8; x++ ) {
			if( now ==0 ) break;
			now--;
			matrix88.drawPixel( x, y, LED_ON );
		}
	}
	matrix88.writeDisplay();
}

// -------------------------------------------------------------------------
// マトリクスLEDの表示関係
// 
int matrix88_x, matrix88_len;
String matrix88_str;

void SetMatrix88( String s ) {
	matrix88_x	 = 0;
	matrix88_len = (s.length()+4) * 5;
	matrix88_str = s;
}

void ScrollMatrix88( void ) {
	Serial.println( matrix88_str );

	matrix88.clear();
	matrix88.setCursor( 8-matrix88_x, 0 );
	matrix88.print( matrix88_str.c_str() );
	matrix88.writeDisplay();

	if( ++matrix88_x > matrix88_len ) matrix88_x =0;
}

void TextMatirx88( String s ) {
	int i;

	matrix88.blinkRate(0);

	for( i=0; i<(s.length()+4)*5; i++ ) {
		matrix88.clear();
		matrix88.setCursor( 8-i, 0 );
		matrix88.print( s.c_str() );
		matrix88.writeDisplay();
		delay(50);
	}
}

// -------------------------------------------------------------------------
// bitmap_matrix88
//  *bmp: アイコンへのポインタ（8x8のint配列）
//  blink : trueで点滅
void BitmapMatrix88( const uint8_t *bmp, bool blink ) {

	matrix88.clear();	// 初期化した後じゃないとblinkRate効かない？★
	matrix88.blinkRate( blink ? 3 : 0 );
	matrix88.drawBitmap(0, 0, bmp, 8, 8, LED_ON );
	matrix88.writeDisplay();
}

// -------------------------------------------------------------------------
// 単品LEDの点滅処理（そもそも必要か？）
// 通常時、１秒おきに点滅を繰り返す
void BlinkPowerLED( void ) {
	static bool led = false;

	// 一応SWを用意するけど、必要か？
	if( blink_led_sw == false ) return;

	led = (led==true)? false : true;
	digitalWrite( kPowerLedPin, led?HIGH:LOW );
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

// -------------------------------------------------------------------------
// ATP3012を使った音声合成
// 一応、音声終了までwaitするようにしてみた★
void Talk( String s, bool wait ) {

	s += ".\r\n";
	Serial2.print( s.c_str() );

	if( wait )	delay(100*s.length());

return;
	char c;

	do {
		c = Serial2.read();

		Serial.print( c );
	} while ( c != '>' );

	Serial.println(".");
}

// -------------------------------------------------------------------------
// WiFiの接続状況をチェックする
// 本システムではWiFi接続は必須のため、未接続の場合は永久に待つ
// loop関数から毎回呼び出して確実にチェックする
void CheckWifi( void ) {
	String ssid, name, ip;

	// まず接続チェック（接続済みならreturn）
	if( WiFi.status() == WL_CONNECTED ) return;

	// 接続処理開始
	BitmapMatrix88( kWiFiBmp, true );
//	Talk( "wa'ifa'iwo sagashiteimasu.",false );

	while( wifi_multi.run() !=WL_CONNECTED ) {
		delay(500);
		Serial.print(".");
	}
	Serial.println( "WiFi Connected." );
	ip =  WiFi.localIP().toString();

	Serial.print( "SSID: " );
	Serial.println( WiFi.SSID() );
	Serial.print( "IPv4: " );
	Serial.println( ip );

	delay(1000);
	ssid = WiFi.SSID();
	if( ssid==SSID1 ) {ssid = "kaishano a'ifon"; name="iPhone"; }
	if( ssid==SSID2 ) {ssid = "jibu'nnno ekusupe'ria"; name="XPERIA"; }
	if( ssid==SSID3 ) {ssid = "ie'no wa'ifa'i"; name="home"; }

	ssid += "ni setuzoku shimasita.";
	Talk( ssid, false );
	TextMatirx88( name.c_str() );
	TextMatirx88( ip  .c_str() );
}

// -------------------------------------------------------------------------
// OTA(On The Air設定)のための書設定
// クラスとメソッド定義だけ
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
// １秒単位のシステムTICKをカウントアップする
// タイマーハンドラーで１秒単位に実行
// ついでにLEDも点滅
void CountSystemTickSec( void ) {
	system_timer_sec++;

	// 底面の電源LEDをブリンクさせる
	// matrix88は自前のタイマでブリンクするので対応不要
	BlinkPowerLED();
}

int SystemTickSec( void ) { return( system_timer_sec ); }

// -------------------------------------------------------------------------
// 揺れを検出した際の処理ハンドラ（割り込みで呼ばれる）
// 割り込み処理はさっさと早く終わらせたいので、フラグだけ立てて終了
// 高速処理が必要とかで、内部RAMに置くための特別の宣言が必要
void IRAM_ATTR onShakeHandler( void ) {
	//static int count=0;
	vibration_detected = true;
	//Serial.print("SHAKE!:");
	//Serial.println( count++ );
}


