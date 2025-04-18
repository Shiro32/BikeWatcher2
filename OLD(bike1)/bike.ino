/*
  bike.ino
  目的：バイク盗難防止アラーム（ESP32用）
  対象：ESP32（Expressifの元祖、ESP32 devkit）

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

  TODO:
  ・振動センサの反応が過敏なのでタイマーなどで緩和したい

*/

// 依存ファイル
#include <uTimerLib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WifiMulti.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include "bike.h"
#include "led.h"
#include "html.h"

// 通信関係
// TODO: WiFiはマルチSSIDにしておかなく必要あり
WiFiMulti wifi_multi;

// システムの動作状態
// WAITING	: 監視開始をタイマで待っている状態（鍵かけなどの時間）
// STANDBY	: ブラウザからの開始コマンドを待っている状態（再稼働）
// RESUME	: 起動直前。自動的にRUNNINGへ遷移
// RUNNING	: システムの監視が始まっている状態
// DETECTED	: 揺れを検知した状態。ブラウザで止めるまで永久
// STOP		: 停止状態（どんな状態なのか・・・！？）


typedef enum { WAITING, STANDBY, RESUME, RUNNING, DETECTED, STOP } SYSTEM_MODE;
SYSTEM_MODE system_mode;	// 初期化はsetupの中で

volatile bool vibration_detected = false;	// false:通常, true:検出
int vibration_start = 0;					// 揺れの継続時間を測るため、揺れていなかった時代の最後のTick（秒）

// -------------------------------------------------------------------------
// 起動時の初期化関数
// ESP32の起動時に１回だけ実行される
// 音声（serial）、LED（I2C）、各種GPIO、割り込みハンドラの設定
void setup( void ) {

	// シリアル通信関係
	Serial.begin(115200);	// PCとのシリアル通信（いるのか？）
	Serial2.begin(9600);	// 音声合成ICとのシリアル通信
	delay( 1000 );
	Serial.println("Welcome!!");
	Talk( "? ima jyunbichuu desu", false );

	// GPIOの設定
	pinMode( kVibrationPin, INPUT_PULLUP );	// 振動センサー入力ピン設定（割り込みハンドラ登録）
	pinMode( kPowerLedPin, OUTPUT );		// 動作中表示用LED制御ピン（底面の1LED）

	// マトリクスLEDの初期化
	InitMatrix88();
	TextMatirx88( "Hi!" );

	// WiFi接続処理
	wifi_multi.addAP( SSID1, PSWD1 );
	wifi_multi.addAP( SSID2, PSWD2 );
	wifi_multi.addAP( SSID3, PSWD3 );

	CheckWifi();	// さっそくWifiに接続する
	SetupOTA();	// ついでにOTA（On The Air設定）も稼働させる

	// Webサーバー初期化
	InitServer();

	// システムタイマ起動（100万マイクロ秒＝１秒）
	TimerLib.setInterval_us( CountSystemTickSec, 1000000 );

	// 初期状態
	// 以前はSTANDBYから始めたけど手間だけかかる（スマホ操作が必要）
	// いきなり監視始められるようにする
	// ★鍵をかける前に始まってしまう問題はどうする？（タイマが必要）
	system_mode = WAITING;
}


// -------------------------------------------------------------------------
// メインルーチン
// 毎回、システムから呼び出される
// 将来的には、DeepSleepを実装したいところ
void loop( void ) {
	// 各種タイミング計測用カウンタ
	static unsigned long standby_counter_s = 0;
	static unsigned long waiting_timer_s = 0;
	static unsigned long detected_counter_s = 0;

	// for OTA
	ArduinoOTA.handle();

	// 5秒に１回、WiFiチェック。切断時は自動リトライ
	if( SystemTickSec() % 5 == 0 ) CheckWifi();

	server.handleClient();	// WEBアクセスへのレスポンス

	// システム状態に応じて処理し、条件がそろえば次の状態に遷移
	switch( system_mode ) {

		case STANDBY: // -- ブラウザでの接続待機中
			// この中では何もせず、ブラウザからの指示でRESUMEに遷移

			// 待機中は振動感知しない
			detachInterrupt( kVibrationPin );

			// 60秒に1度、接続を促す
			if( WaitSec( &standby_counter_s, 60 ) ) {
				Talk( "burau'zade kai'sibo'tanwo osite'kudasai.", false );
				Serial.println( "Please connect via browser." );
				delay(1000);	// これを入れないと数十回呼ばれてしまう
			}
			// 接続待ちの表示
			BitmapMatrix88( kWaitBmp, true );
			break;

		case WAITING: // -- 起動タイマーで待たされている状態
			// 振動検出しない
			detachInterrupt( kVibrationPin );

			// 準備中の表示（バー表示）
			IndicatorMatrix88( SystemTickSec(), kStartMonitorTimer_s );

			// 起動準備完了
			if( WaitSec( &waiting_timer_s, kStartMonitorTimer_s ) ) {
				system_mode = RESUME;
				Serial.println("MONITORING Start!" );
			}
			break;

		case RESUME: // -- 起動直前。停止からの再開ポイント
			system_mode = RUNNING;

			Serial.println("MONITORING Start!" );

			// 振動センサ起動
			vibration_detected = false;
			attachInterrupt( kVibrationPin, onShakeHandler, CHANGE );
			BitmapMatrix88( kSmileBmp, true );
			break;

		case RUNNING: // -- 起動完了後の処理（通常ループ）
			BitmapMatrix88( kSmileBmp, true );
			
			// 震動を検出し続けている限り、無数に呼び続けられる
			// 都度、震動フラグをオフにして、振動センサで再セットされる回数をカウント
			if( vibration_detected ) {
				vibration_detected = 0;	// リセットして、センサーで再セットされることを待つ
				delay(1000);

				Serial.print("DETECTING:start=");
				Serial.print( vibration_start );
				Serial.print( "/now=");
				Serial.println( SystemTickSec() );

				// 一定時間以上揺れ続けたら、「震動」とみなす
				if( SystemTickSec() - vibration_start >= kVibrationTimer_s ) {
					system_mode = DETECTED;

					// 警告音声
					Talk( "shindouwo kenshutusimasita. nusu'nja dame'desuyo'.", true);

					// お怒り表示
					BitmapMatrix88( kBikkuriBmp, true );
					Serial.println( "振動検出!" );

					// LINEで警報通知
					SendLineNotify("自転車が盗まれそうになっていますよ！ はやく戻って確認しましょう！");
				}
			} else {
				vibration_start = SystemTickSec();	// 揺れていなかった頃の時刻を覚えておく
			}

			break;

		case DETECTED: // -- 振動検出後の処理（webで停めるまでずっと）
			static int a = 0;
			BitmapMatrix88( kXBmp, true );
			if( WaitSec( &detected_counter_s, 30 ) ) Talk( (++a % 2)==0?"nusu'nja dame'desuyo'":"shindouwo kenshutu simasita.", false );
			break;

		case STOP: // -- 終了～
			// 待機中は振動感知しない
			detachInterrupt( kVibrationPin );

			BitmapMatrix88( kWaitBmp, false );
			break;

	} // case
} // loop

bool WaitSec( unsigned long* prev, int timer_s ) {
	
	// 初めての呼び出し時には現在時刻を記憶
	if( *prev==0 ) *prev = millis();

	if( millis() - *prev >= timer_s*1000 ) {
		*prev = 0;
		return true;
	} else {
		return false;
	}
}


// 高度な（？）時間待ち関数の積もりだったけど、%(mod)で代用できるので出番なし
bool WaitSec2( int sec ) {
	static int begin=0;

	if( begin==0 ) begin = SystemTickSec();
	
	if( SystemTickSec() - begin >= sec ) {
		begin = 0;
		return true;
	} else {
		return false;
	}

}



