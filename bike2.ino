/*
  bike2.ino
  目的：バイク盗難防止アラーム２号機（twelite）
  対象：XIAO ESP32C

  履歴：
  2025/05/12 初版稼働（WiFiの動的切替、液晶表示）

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

// 自作ヘッダファイル
#include "bike2.h"
#include "sub.h"
#include "html2.h"

// 各種モード保持グローバル（初期化はsetupで）
SYSTEM_MODE	gSystemMode;	// 待機・検出などの全体モード
COMM_MODE	gCommMode;		// 通報モード（DIRECT: 親機を持ち歩きtweliteで通知 / WIFI:親機をバイクに置いてWIFI通知）

// OLED関係
OLED oled( OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET );

// -------------------------------------------------------------------------
// 起動時の初期化関数
// OLED（I2C）、各種GPIO、割り込みハンドラの設定
void setup( void ) {
	String ip;

	// シリアル通信関係
	Serial.begin(115200);	// PCとのシリアル通信（いるのか？）
	
	// OLEDの初期化
	if(!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
		for(;;);
	}

	// 起動画面
	oled.clear();
	oled.setRotation(2);
	oled.drawXBitmap( 0, 0, ICON_BA2LOGO, 128, 64, WHITE );
	oled.flush();

	// ボタンGPIO
	SetupButtons();

	// まずとりあえずはWiFiとOTAを動かす
	SetupWiFi();
	ConnectWiFi();
	SetupOTA();

	// 通信モードを選択してもらう
	SelectCommMode();

	// それぞれのモードに応じた初期化処理
	switch( gCommMode ) {
	  case WIFI_MODE:
		// IPの表示
		ip = WiFi.localIP().toString();
		ip = "IP:"+ip;
		oled.print( 0, 30, 1, ip.c_str(), true );
		oled.flush();

		// Webサーバー初期化
		InitServer();
		oled.flush();
		break;
	
	  case DIRECT_MODE:
		DisconnectWiFi();	// 不要なWIFIを停止
		oled.drawXBitmap(0, 0, ICON_MONITORING, 128, 64, WHITE);
		oled.flush();
		break;
	}	

	// バッテリー残量管理
	SetupBattSOC();

	// 操作用ボタン初期化
	SetupButtons();

	oled.clear();
	gSystemMode = WAIT_STD;
}

// -------------------------------------------------------------------------
// メインルーチン
// 毎回、システムから呼び出される
// 将来的には、DeepSleepを実装したいところ
void loop( void ) {
	// 各種タイミング計測用カウンタ
	static uint32_t oneSecTimer_s = 0;			// １秒をWaitSecで作るカウンタ
	static uint32_t standbyCounter_s = 0;		
	static uint32_t wifi_counter_s = 0;
	static uint32_t displayUpdateTimer_s = 0;	// ゆっくりした画面表示更新に使うカウンタ

	static uint16_t waitTimerLimit_s = 0;		// 起動時の待ち時間上限
	static uint16_t waitTimerBegin_s = 0;		// 起動時の待ち時間タイマー

	char s[80];
	delay(200);

	// 1秒ごとのシステムtickをカウントUPする（タイマ割込みだとハングするので）
	if( WaitSec( &oneSecTimer_s, 1) ) CountSystemTickSec();


	// WiFiモード時のネットワーク関係処理（DIRECTモードでは特段の処理不要）
	if( gCommMode==WIFI_MODE ) {
		// 5秒に１回、WiFiチェック。切断時は自動リトライ
		if( WaitSec(&wifi_counter_s, 5) ) ConnectWiFi();

		// WEBアクセスへのレスポンス（警報停止など）
		server.handleClient();
	}

	// システム状態に応じて処理し、条件がそろえば次の状態に遷移
	switch( gSystemMode ) {

	  case WAIT_SHORT:
		waitTimerLimit_s = 5;
		waitTimerBegin_s = SystemTickSec();
		gSystemMode = WAITING;
		break;

	  case WAIT_STD:
		waitTimerLimit_s = START_MONITORING_TIMER_s;
		waitTimerBegin_s = SystemTickSec();
		gSystemMode = WAITING;
		break;

	  case WAITING: // -- 起動タイマーで待たされている状態
		// 振動検出しない
		DetachTW2525Interrupt();
		
		// 準備中のカウントダウン
		sprintf( s, "%u", waitTimerLimit_s-(SystemTickSec()-waitTimerBegin_s) );
		oled.clear();
		oled.print( OLED_WIDTH/2, 10, ALIGN_CENTER, 5, s );
		oled.flush();

		// どちらのボタンが押されても残り5秒に移行
		if( leftBtnStatus==BTN_1CLICK || rightBtnStatus==BTN_1CLICK ) {
			leftBtnStatus  = BTN_NOTHING;
			rightBtnStatus = BTN_NOTHING;
			gSystemMode = WAIT_SHORT;
			break;
		}

		// タイムアップ→RESUMEに移行
		if( SystemTickSec()-waitTimerBegin_s >= waitTimerLimit_s ) {
			gSystemMode = RESUME;
			Serial.println("MONITORING Start!" );
		}
		break;

	  case RESUME: // -- 起動直前（停止からの再開ポイント）すぐに移行
		Serial.println("MONITORING Start!" );

		// 振動センサ起動
		AttachTW2525Interrupt();

		oled.clear();
		gSystemMode = RUNNING;
		break;

	  case RUNNING: // -- 通常ループ処理（振動信号を待ち、待ち受け画面を作る）
			
		// WIFIモードの画面（親機も自転車にある）
		if( gCommMode==WIFI_MODE ) {
			// 監視中アイコンを点滅表示
			if( SystemTickSec()%2 )
				oled.drawXBitmap( 0, 0, ICON_MONITORING, 128, 64, WHITE );
			else
				oled.clear();
		}
		// DIRECTモードの画面
		else {
			sprintf( s, "PAST:%lus", SystemTickSec() );
			oled.print( 0,10, ALIGN_LEFT, 2, s );

			if( WaitSec(&displayUpdateTimer_s, 5) ) {
				sprintf( s, "BATT:%1.1fV", GetBattSoc() );
				oled.print( 0,40, ALIGN_LEFT, 2, s );
			}
		}
		oled.flush();

		// ボタンを押された時の処理
		// 現状でやることはないが、処理しておかないとバッファにたまってしまう
		if( leftBtnStatus!=BTN_NOTHING && rightBtnStatus!=BTN_NOTHING ) {
			leftBtnStatus  = BTN_NOTHING;
			rightBtnStatus = BTN_NOTHING;
		}

		// 振動検出時の対応（検出自体は割り込み処理でgVibrationDetectedフラグで知る）
		if( gVibrationDetected!=0 ) {
			Serial.println( "Detected!" );
			gSystemMode = DETECTED;
			if( gCommMode==WIFI_MODE ) SendLineNotify("振動検出！すぐ自転車に戻りましょう！");

			oled.clear();
			sprintf( s, "D%d!!", gVibrationDetected );
			oled.print( OLED_WIDTH/2, 10, ALIGN_CENTER, 5, s );
			oled.flush();
			delay(3000);

			// 振動検出をクリアして次の発生に備える
			gVibrationDetected = 0;
		}
		break;

	  case DETECTED: // -- 振動検出後の処理（webで停めるまでずっと）
		oled.clear();

		// WIFIモードの表示（親機も自転車にある）
		if( gCommMode==WIFI_MODE ) {
			if( SystemTickSec()%2 ) {
				oled.drawXBitmap( 0, 0, (SystemTickSec()%10<5)?ICON_ALERT:ICON_ALERT2, 128, 64, WHITE );
			}
		}
		// DIRECTモードの表示
		else {
			if( SystemTickSec()%2 ) oled.drawXBitmap( 0, 0, ICON_ALERT, 128, 64, WHITE );
		}
		oled.flush();

		// 警報停止処理
		if( leftBtnStatus==BTN_1CLICK || rightBtnStatus==BTN_1CLICK ) {
			leftBtnStatus  = BTN_NOTHING;
			rightBtnStatus = BTN_NOTHING;
			
			oled.clear();
			oled.print( OLED_WIDTH/2, 10, ALIGN_CENTER, 5, "STOP" );
			oled.flush();
			delay(3000);

			gSystemMode = WAIT_SHORT;
		}
		break;

	  case STOP: // -- 終了～
		// 待機中は振動感知しない
		DetachTW2525Interrupt();

		// システム終了的なアイコン表示
		oled.clear();
		//oled.drawXBitmap( 0, 0, ICON_MONITORING, 128, 64, WHITE );
		oled.flush();
		break;

	} // case

} // loop
