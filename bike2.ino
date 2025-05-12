/*
  bike2.ino
  目的：バイク盗難防止アラーム２号機（twelite）
  対象：XIAO ESP32C

  履歴：
  2022/10/16 初版稼働（WiFiの動的切替、音声発信など）

  TODO:

*/

// 依存ファイル
#include <uTimerLib.h>
#include <Wire.h>


// for OTA
#include <WiFi.h>
#include <WifiMulti.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>

#include <WebServer.h>

// for OLED Display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#include "bike2.h"
#include "sub.h"
#include "html2.h"

// 各種モード保持グローバル（初期化はsetupで）
SYSTEM_MODE gSystemMode;	// 待機・検出などの全体モード
COMM_MODE gCommMode;		// 通報モード（DIRECT: 親機を持ち歩きtweliteで通知 / WIFI:親機をバイクに置いてWIFI通知）

// OLED関係
OLED oled( OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET );

float gBattSOC;								// バッテリー残量（電圧？）

// -------------------------------------------------------------------------
// 起動時の初期化関数
// ESP32の起動時に１回だけ実行される
// OLED（I2C）、各種GPIO、割り込みハンドラの設定
void setup( void ) {
	String ip;

	// シリアル通信関係
	Serial.begin(115200);	// PCとのシリアル通信（いるのか？）
	
	// OLEDの初期化
	if(!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
		for(;;);
	}

	oled.clear();               //何か表示されている場合に備えて表示クリア
	oled.setRotation(2);
	oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, 2, "Welcome" );
	oled.drawXBitmap( 0, 0, ICON_ALERT, 128, 64, WHITE );
	oled.flush();

	// ボタンGPIO
	SetupButtons();

	// まずとりあえずはWiFiとOTAを動かす
	SetupWiFi();
	ConnectWiFi();
	SetupOTA();

	// 通信モードを選択してもらう
	SelectCommMode();

	switch( gCommMode ) {
	  case WIFI_MODE:
		oled.print( 0, 20, ALIGN_LEFT, 1, "Checking Network..." );
		oled.flush();

		// Webサーバー初期化
		InitServer();

		ip = WiFi.localIP().toString();
		ip = "IP:"+ip;
		oled.print( 0, 30, 1, ip.c_str(), true );
		oled.flush();
		break;
	
	  case DIRECT_MODE:
		DisconnectWiFi();
		oled.drawXBitmap(0, 0, ICON_MONITORING, 128, 64, WHITE);
		oled.flush();
		break;
	}	

	// バッテリー残量管理
	SetupBattSOC();
	gBattSOC = GetBattSoc();

	// 操作用ボタン初期化
	SetupButtons();

	// 初期状態
	// 以前はSTANDBYから始めたけど手間だけかかる（スマホ操作が必要）
	// いきなり監視始められるようにする
	// ★鍵をかける前に始まってしまう問題はどうする？（タイマが必要）
	gSystemMode = WAITING;

	// システムタイマ起動（100万マイクロ秒＝１秒）
	TimerLib.setInterval_us( CountSystemTickSec, 1000000 );

	oled.clear();               //何か表示されている場合に備えて表示クリア

}


// -------------------------------------------------------------------------
// メインルーチン
// 毎回、システムから呼び出される
// 将来的には、DeepSleepを実装したいところ
void loop( void ) {
	// 各種タイミング計測用カウンタ
	static uint32_t standby_counter_s = 0;
	static uint32_t waiting_timer_s = 0;
	static uint32_t detected_counter_s = 0;
	static uint32_t wifi_counter_s = 0;

	char s[80];

	// WiFiモード時のネットワーク関係処理
	if( gCommMode==WIFI_MODE ) {
		// OTA処理
		ArduinoOTA.handle();

		// 5秒に１回、WiFiチェック。切断時は自動リトライ
		if( WaitSec(&wifi_counter_s, 5) ) ConnectWiFi();

		// WEBアクセスへのレスポンス（警報停止など）
		// server.handleClient();
	}

	// システム状態に応じて処理し、条件がそろえば次の状態に遷移
	switch( gSystemMode ) {
#if 0	
		case STANDBY: // -- ブラウザでの接続待機中
			// この中では何もせず、ブラウザからの指示でRESUMEに遷移

			// 待機中は振動感知しない
			DetachiTW2525Interrupt();
			break;
#endif

		case WAITING: // -- 起動タイマーで待たされている状態
			// 振動検出しない
			DetachTW2525Interrupt();
			
			// 準備中のカウントダウン
			sprintf( s, "%2lu", START_MONITORING_TIMER_s - SystemTickSec() );
			oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, 5, s );
			oled.flush();

			if( SystemTickSec()>=START_MONITORING_TIMER_s ) {
				gSystemMode = RESUME;
				Serial.println("MONITORING Start!" );
			}
			break;

		case RESUME: // -- 起動直前。停止からの再開ポイント
			gSystemMode = RUNNING;

			Serial.println("MONITORING Start!" );

			// 振動センサ起動
			AttachTW2525Interrupt();
			break;

		case RUNNING: // -- 起動完了後の処理（通常ループ）
			oled.clear();
			
			if( gSystemMode==WIFI_MODE ) {
				if( SystemTickSec()%2 )	oled.drawXBitmap( 0, 0, ICON_MONITORING, 128, 64, WHITE );
			} else {
				sprintf( s, "monitoring %lu sec", SystemTickSec() );
				oled.print( 0, 0, ALIGN_LEFT, 1, s );
				sprintf( s, "batt voltage:%1.2f", GetBattSoc() );
				oled.print( 0,20, ALIGN_LEFT, 1, s );
			}
			oled.flush();

			if( gVibrationDetected ) {
				Serial.println( "Detected!" );
				gSystemMode = DETECTED;
				SendLineNotify("振動検出！すぐ自転車に戻りましょう！");
			}
			break;

		case DETECTED: // -- 振動検出後の処理（webで停めるまでずっと）
			//static int a = 0;
			//BitmapMatrix88( kXBmp, true );
			//if( WaitSec( &detected_counter_s, 30 ) ) Talk( (++a % 2)==0?"nusu'nja dame'desuyo'":"shindouwo kenshutu simasita.", false );
			break;

		case STOP: // -- 終了～
			//// 待機中は振動感知しない
			//detachInterrupt( kVibrationPin );

			//BitmapMatrix88( kWaitBmp, false );
			break;

	} // case



} // loop

