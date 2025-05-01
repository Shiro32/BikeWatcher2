// bike.inoで使うヘッダファイル

#ifndef _BIKE_H_
#define _BIKE_H_

// システムモード
// WAITING	: 監視開始をタイマで待っている状態（鍵かけなどの時間）
// STANDBY	: ブラウザからの開始コマンドを待っている状態（再稼働）
// RESUME	: 起動直前。自動的にRUNNINGへ遷移
// RUNNING	: システムの監視が始まっている状態
// DETECTED	: 揺れを検知した状態。ブラウザで止めるまで永久
// STOP		: 停止状態（どんな状態なのか・・・！？）
typedef enum { WAITING, STANDBY, RESUME, RUNNING, DETECTED, STOP } DETECTION_STATUS;

// 通報方式
typedef enum { DIRECT_MODE, WIFI_MODE } COMM_MODE;


// OLEDデバイス設定
#define OLED_WIDTH		128		//解像度 128 x 64 で使用します。
#define OLED_HEIGHT		64
#define OLED_RESET		-1		//使用しないので　-1を設定する。
#define OLED_ADDRESS	0x3C	//I2Cアドレスは 0x3C



// WIFI接続関係（プレーンテキストはまずい気もするが・・・）
#define SSID1	"Shiro-iPhone"
#define PSWD1	"hogehoge"

#deffne SSID2	"C330"
#define PSWD2	"hogehoge"

#define SSID3	= "TP_LINK_KURA_5G"
#define PSWD3	= "0466831703"

#define LINE_TOKEN	= "zG7RzSGz0lore6SNGqF2VrFcVX1r2OcKGezJq7moAVQ"
#define LINE_HOST	= "notify-api.line.me"

// GPIO関係（あんまり無いけど）
#define GPIO_BAT_SOC	A0
#define GPIO_BTN_LEFT	D1
#define GPIO_BTN_RIGHT	D2

#define GPIO_TW2525_D1	D3
#define GPIO_TW2525_D2	D4
#define GPIO_TW2525_D3	D5
#define GPIO_TW2525_D4	D6


// システム動作関係
const int kStartMonitorTimer_s = 30;	// （秒）監視開始タイマ（3分くらいがいい？）
const int kVibrationTimer_s = 2;		// どれくらい継続したら震動検知とするか（秒）


// ボタン関係
extern volatile BTN_STATUS leftBtnStatus, rightBtnStatus;

void onShakeHandler();

#endif

