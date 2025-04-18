/*
プロジェクト：超小型CO2計測ガジェット
動きなどなど：
・USB Cで動作
・LiPoも内蔵している（XIAOのBMS機能を活用）
・操作はボタン２個

〇動作モード
　・トレンドグラフ
　・数字のみ
　（どちらも大して変わらんけど・・・）

〇動作フラグ
　・クラウドアップロード（ambientにアップする）
　・ロギング（内蔵のflashに書き込む）

〇操作系
・B1（左）
　クリック：モード切替（トレンド→数字→トレンド・・・）
　プレス　：ロギング掃き出し（やり方しらん）

・B2（右）
　クリック：ロギングSW
　プレス　：クラウドSW

・MHZCO2ライブラリ
https://github.com/RobTillaart/MHZCO2

・より詳細説明
https://robot-jp.com/wiki/index.php/Parts:Sensor:MH-Z19C

*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MHZCO2.h>
#include <HardwareSerial.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// for OTA
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "sub.h"
#include "co3.h"

// OLED関係
OLED oled( OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET );

// CO2関係（かなり難航したけど、このライブラリで行こう）
// https://github.com/RobTillaart/MHZCO2
MHZ19B			mhz19;			// CO2センサーのインスタンス
HardwareSerial	MySerial0(0);	// USBシリアルとは別にGPIOのシリアル（6, 7pin）

// 全体モードなど
SYSTEM_MODE gSystemMode;	// 初期化はsetupの中で

SYSTEM_MODE gNextSystemMode[] = {
	MODE_GRAPH_PRE		, MODE_GRAPH_PRE,		// Num　→　Graph
	MODE_NUM_PRE		, MODE_NUM_PRE,			// Graph→　Num

	MODE_CLOUD_PRE		, MODE_CLOUD_PRE,		// LOG　→　CLOUD
	MODE_CALIBRATE_PRE	, MODE_CALIBRATE_PRE,	// CLOUD　→　CALIBRATE
	MODE_LOG_PRE		, MODE_LOG_PRE			// CALIBRATE　→　CLOUD
};

// グラフレンジ
RANGE_INFO gRangeInfo[] = {
	{"60sec"	,"60s"	,"30"	,30		,15},
	{"10min"	,"10m"	,"5"	,10		,5},
	{"6hour"	,"6h"	,"3"	,360	,180},
	{"12hour"	,"12h"	,"6"	,720	,360}
};

uint8_t gRange = 1;	// 現在のレンジモード

// CO2記録バッファ
#define CO2_BUFFER_SIZE 		800	// グラフレンジにかかわらず最大幅で確保しておく
#define CO2_SENSE_BUFFER_SIZE	100	// 1分バッファ
uint16_t gCO2Buffer[ CO2_BUFFER_SIZE ];
uint16_t gCO2SenseBuffer[ CO2_SENSE_BUFFER_SIZE ];
uint16_t gNewestCO2=400;

// ログ関係スイッチ
bool gLogSW;		// flashメモリにログデータを蓄積するかどうか
bool gCloudSw;		// クラウド（ambient）へアップするかどうか
float gBatSOC;


// -------------------------------------------------------------------------
// グラフの枠組みを描く
// graphPadX：左の余白（可変にする意味あまりないかも）
uint8_t chartX1, chartX2, chartW;
uint8_t chartY1, chartY2, chartH, chartCenterY;

void SetupGraph( int graphPadX ) {
	// graph:グラフエリア、chart:チャートエリア
	// （x1, y1):左上、(x2, y2)：右下, w:幅, h:高さ
	// チャートエリア（横）
	chartX1	= graphPadX + CHART_PAD_X;
	chartX2	= OLED_WIDTH;
	chartW	= chartX2- chartX1;

	// チャートエリア（縦）
	chartY1 = 0;
	chartY2 = OLED_HEIGHT - CHART_PAD_Y;
	chartH	= chartY2 - chartY1;
	chartCenterY = (chartY2 - chartY1)/2;

	// ラベル・キャプション
	oled.print( graphPadX, 00, 1, "2K", false );
	oled.print( graphPadX, 25, 1, "1K", false );
	oled.print( graphPadX, 50, 1, " 0", false );

	uint8_t half_x = 93 - strlen( gRangeInfo[gRange].half_name )*5/2;
	oled.print( graphPadX+12	, 57, 1, gRangeInfo[gRange].max_name	, false );
	oled.print( half_x		, 57, 1, gRangeInfo[gRange].half_name	, false );
	oled.print( 122			, 57, 1, "0"							, false );

	// チャートエリアを描く
	oled.fillRect( chartX1, chartY1, chartW, chartH, SSD1306_BLACK );
	oled.drawRect( chartX1, chartY1, chartW, chartH, SSD1306_WHITE );
	oled.drawLine( chartX1, chartCenterY, chartX2, chartCenterY, SSD1306_WHITE );
}

// -------------------------------------------------------------------------
// グラフを描く
// 枠組みはSetupで書いているので、チャートの中身だけ描く
void DrawGraph() {
	uint8_t x, y, ox, oy;
	uint16_t i, xrange;
	uint16_t *bufPtr;

	// チャートエリアを描く
	oled.fillRect( chartX1+1, chartY1+1, chartW-2, chartH-2, SSD1306_BLACK );
	oled.drawLine( chartX1, chartCenterY, chartX2, chartCenterY, SSD1306_WHITE );

	// 通常グラフと高速グラフ
	bufPtr = (gRange==0)? gCO2SenseBuffer : gCO2Buffer;

	// いよいよチャートを描く
	xrange = gRangeInfo[gRange].max_size;
	ox = 0;
	oy = *bufPtr * (double)chartH / (CHART_MAX_Y-CHART_MIN_Y);

	for( i=0; i<=xrange; i++ ) {
		x = (double)i * chartW / xrange;
		y = (double)*bufPtr * chartH / (CHART_MAX_Y-CHART_MIN_Y);
		bufPtr++;
		oled.drawLine( chartX2-ox, chartY2-oy-1, chartX2-x, chartY2-y-1, WHITE );

		ox = x; oy = y;
	}
}

void DrawRangeInfo() {
	char s[80];
	
	// チャートエリアを描く
	oled.fillRect( chartX1+1, chartY1+1, chartW-2, chartH-2, SSD1306_BLACK );

	oled.print( 80,20,1,"Range" );
	oled.print( 80,30,1,gRangeInfo[gRange].name );
}

// -------------------------------------------------------------------------
// ステータスエリアの枠を描く
void SetupStatus() {
	char s[30];

#if 0
	switch( gSystemMode ) {
	case MODE_NUM:
		break;

	case MODE_GRAPH:
		oled.print( 0,  0, 1, "B:95", false );
		oled.print( 0,  8, 1, gCloudSw?"C:o":"C:x"	, false );
		oled.print( 0, 16, 1, gLogSW?"L:o":"L:x"		, false );
		oled.print( 0, 24, 1, "W:x", false );
		oled.print( 0, 32, 1, "now", false );
		break;
	}

#endif

}
// -------------------------------------------------------------------------
// 周辺のステータスを描画する
void DrawStatus() {
	char co2str[10], socstr[10];
	char s[80];

	switch( gSystemMode ) {
	case MODE_NUM:
	case MODE_NUM_PRE:
		sprintf( s, "B:%1.2f C:%s L:%s W:%s", gBatSOC, 
			gCloudSw?"o":"x", 
			gLogSW?"o":"x",
			"x" );
		oled.print( OLED_WIDTH/2, 55, ALIGN_CENTER, 1, s );

		oled.fillRect( 0,0, OLED_WIDTH-1, 54, BLACK );
		sprintf( s,"%d",gNewestCO2 );
		oled.print( OLED_WIDTH/2, 10, ALIGN_CENTER, 5, s );
		break;

	case MODE_GRAPH:
	case MODE_GRAPH_PRE:
		sprintf( s, "B:%1.2f", gBatSOC );
		oled.print( 0,  0, 1, s );

		oled.print( 0,  8, 1, gCloudSw?	"C:on ":"C:off"	);
		oled.print( 0, 16, 1, gLogSW?	"L:on ":"L:off"	);


		String ip;
		ip = WiFi.localIP().toString();
		ip = ip.substring( ip.length()-3 );
		ip = "W:"+ip;
		oled.print( 0, 24, 1, ip.c_str() );

		oled.print( 0, 38, 1, "now" );
		sprintf( s, "%4d", gNewestCO2 );
		oled.print( 0, 50, 2, s );
		break;
	}
}


// -------------------------------------------------------------------------
// 起動時の初期化関数
// ESP32の起動時に１回だけ実行される
void setup() {
	Serial.begin(115200);
	Serial.println("Welcome!");

	if(!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
		for(;;);
	}
	oled.clear();               //何か表示されている場合に備えて表示クリア
	oled.setRotation(2);
	oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, 2, "Welcome" );
	oled.flush();

	// WIFI関係
	oled.print( 0, 20, ALIGN_LEFT, 1, "Checking Network..." );
	oled.flush();

	SetupWiFi();
	SetupOTA();
	SetupCloud();

	String ip;
	ip = WiFi.localIP().toString();
	ip = "IP:"+ip;
	oled.print( 0, 30, 1, ip.c_str(), true );
	oled.flush();

	// モード
	gSystemMode = MODE_NUM_PRE;
	gLogSW = false;
	gCloudSw = false;

	SetupBatSOC();
	gBatSOC = GetBatSoc();
	SetupButtons();

	// CO2初期化
	delay( 1*1000 );
	MySerial0.begin(9600);
	oled.print( 0, 40, 1, "Now heating...", true );
	oled.flush();

	delay( 1*1000 );
	mhz19.begin(&MySerial0);
	mhz19.setPPM(5000);
	mhz19.calibrateAuto(false);
	for( int i=0; i<CO2_BUFFER_SIZE; i++ ) gCO2Buffer[i]=0;
	for( int i=0; i<100; i++ ) gCO2SenseBuffer[i] = 0;

	oled.flush();
}

// -------------------------------------------------------------------------
// メインルーチン
// 毎回、システムから呼び出される
void loop() {
	char s[50];
	static uint32_t sensing_timer_ms = 0;
	static uint32_t logging_timer_ms = 0;
	static uint32_t bat_soc_timer_ms = 0;
	static uint32_t graph_timer_ms = 0;

	static uint16_t co2s_ppm = 0, co2_count = 0;

	uint16_t co2_ppm, temp_c;
	static uint8_t err_count = 0;

	delay(10);

	// OTA
	ArduinoOTA.handle();

	// センサーリード（=2秒）
	if( WaitSec(&sensing_timer_ms, SENSING_TIMER_s ) ) {
		mhz19.measure();
		//uint16_t temp_c = 	mhz19.getTemperature();
		uint16_t co2_ppm = 	mhz19.getCO2();

		// 起動直後、CO2センサーがエラー値を連続してしまうことがあり、
		// ソフトウェアリセットしないと回復しないための処理
		if( co2_ppm<300 || co2_ppm>5000 ) {
			if( ++err_count>=2 ) {
				oled.clear();
				oled.print( 20, 30, 2, "RESTART!" );
				oled.flush();
				delay(2*1000);
				ESP.restart();
			}
		} else {
			err_count = 0;
		}

		// 高速1分バッファを更新しておく
		// 2秒ごとに全部スクロールは何とかしないと・・・。
		for( uint16_t i=99; i>0; i-- ) gCO2SenseBuffer[i] = gCO2SenseBuffer[i-1];
		gCO2SenseBuffer[0] = co2_ppm;

		co2s_ppm += co2_ppm;
		co2_count++;

		// 最速モード時は常にグラフ更新
		if( gSystemMode==MODE_GRAPH && gRange==0 ) DrawGraph();

		// ステータスエリア更新
		gNewestCO2 = co2s_ppm / co2_count;
		DrawStatus();
		oled.flush();
	}


	// バッファ更新・SPIFF書き込み・AMBIENT送信（＝60秒間隔）
	if( WaitSec(&logging_timer_ms, LOGGING_TIMER_S ) ) {
		// リングバッファ更新
		// この時に最大・最小チェックがいい
		for( uint16_t i=CO2_BUFFER_SIZE-1; i>0; i--) gCO2Buffer[i] = gCO2Buffer[i-1];
		gCO2Buffer[0] = co2s_ppm / co2_count;
		co2_count = 0;
		co2s_ppm = 0;

		// SPIFFへの書き込み
		// AMBIENTへの送信
		if( gCloudSw ) SendToCloud( gNewestCO2 );

		// ステータスエリア更新
		DrawStatus();
		oled.flush();
	}


	// バッテリーチェック
	if( WaitSec(&bat_soc_timer_ms, BAT_SOC_TIMER_S) ) {
		gBatSOC = GetBatSoc();
		DrawStatus();
		oled.flush();
	}

	// 各種スイッチ操作対応
	// 1clickは単なるモード切替なのでgNextSystemModeだけで対応できる
	if( leftBtnStatus==BTN_1CLICK ) {
		gSystemMode = gNextSystemMode[gSystemMode];
		delay(500);
		leftBtnStatus = BTN_NOTHING;
	}
	// ロングプレスは、表示・設定モードを切り替える
	if( leftBtnStatus==BTN_LONGPRESS ) {
		if( gSystemMode<=MODE_GRAPH )
			gSystemMode=MODE_LOG_PRE;
		else
			gSystemMode=MODE_NUM_PRE;
		delay(500);
		leftBtnStatus = BTN_NOTHING;
	}


	// 状態遷移
	switch( gSystemMode ){
	//--------------------------------------------------------------------------
	case MODE_NUM_PRE:
		oled.clear();
		SetupStatus(); DrawStatus();
		oled.flush();
		gSystemMode = MODE_NUM;
		graph_timer_ms += 60*1000;

	case MODE_NUM:
		//if( WaitSec(&graph_timer_ms, GRAPH_TIMER_S ) ) {
		//}
		break;

	//--------------------------------------------------------------------------
	case MODE_GRAPH_PRE:
		oled.clear();
		SetupGraph( GRAPH_PAD_X );
		SetupStatus(); DrawStatus();
		DrawRangeInfo();
		oled.flush();
		delay( 1*1000 );
		gSystemMode = MODE_GRAPH;
		graph_timer_ms += 60*1000;

	case MODE_GRAPH:
		if( WaitSec(&graph_timer_ms, GRAPH_TIMER_S ) ) {
			DrawGraph( );
			oled.flush();
		}

		if( rightBtnStatus==BTN_1CLICK ) {
			gRange = ++gRange % 4;
			oled.clear();
			SetupGraph( GRAPH_PAD_X );
			SetupStatus(); DrawStatus();
			DrawRangeInfo();
			oled.flush();
			delay( 2*1000 );
			gSystemMode = MODE_GRAPH;
			graph_timer_ms += 60*1000;
			rightBtnStatus = BTN_NOTHING;
		}
		break;

	//--------------------------------------------------------------------------
	case MODE_LOG_PRE:
		oled.clear();
		oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, 1, "< LOG SETTING >" );
		oled.flush();
		gSystemMode = MODE_LOG;

	case MODE_LOG:
		if( rightBtnStatus==BTN_1CLICK ) {
			gLogSW = gLogSW?false:true;
			delay(500);
			rightBtnStatus = BTN_NOTHING;
		}
		oled.print( OLED_WIDTH/2,20,ALIGN_CENTER,4,gLogSW?" ON ":"OFF" );
		oled.flush();
		break;
	//--------------------------------------------------------------------------
	case MODE_CLOUD_PRE:
		oled.clear();
		oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, 1, "< AMBIENT SETTING >" );
		oled.flush();
		gSystemMode = MODE_CLOUD;

	case MODE_CLOUD:
		if( rightBtnStatus==BTN_1CLICK ) {
			gCloudSw = gCloudSw?false:true;
			delay(500);
			rightBtnStatus = BTN_NOTHING;
		}
		oled.print( OLED_WIDTH/2,20,ALIGN_CENTER,4,gCloudSw?"SEND":"STOP" );
		oled.flush();
		break;
	
	//--------------------------------------------------------------------------
	case MODE_CALIBRATE_PRE:
		oled.clear();
		oled.print( OLED_WIDTH/2, 0, ALIGN_CENTER, 1, "< CALIBRATE SENSOR >" );
		oled.print( 10, 30, 1, "Push right btn" );
		oled.print( 10, 38, 1, "to calibrate!" );
		oled.flush();
		gSystemMode = MODE_CALIBRATE;

	case MODE_CALIBRATE:
		if( rightBtnStatus==BTN_1CLICK ) {
			oled.print( 10, 30, 1, "Now Calibrating...      ");
			oled.flush();
			mhz19.calibrateZero();
			delay(5*1000);
			rightBtnStatus = BTN_NOTHING;
			gSystemMode = MODE_CALIBRATE_PRE;
		}
		break;
	}


}
