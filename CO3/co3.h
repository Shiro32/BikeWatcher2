#define SENSING_TIMER_s	5	// 5
#define LOGGING_TIMER_S	60	// 60
#define GRAPH_TIMER_S	60	// 60

#define BAT_SOC_TIMER_S 60*5


// チャート関係のサイズ（多分苦労する）
// グラフ内部のチャート周辺の隙間
// グラフエリア全体の左マージン（右や上下はフル）
#define GRAPH_PAD_X	48

// ラベル・キャプションのため
#define CHART_PAD_X	12
#define CHART_PAD_Y 8

// 縦軸のレンジ
#define CHART_MIN_Y 0
#define CHART_MAX_Y 2000

// OLED周辺のデバイス設定
#define OLED_WIDTH	128		//解像度 128 x 64 で使用します。
#define OLED_HEIGHT	64
#define OLED_RESET	-1		//使用しないので　-1を設定する。
#define OLED_ADDRESS	0x3C	//I2Cアドレスは 0x3C


// ピン番号（あんまりないけど）
#define GPIO_BAT_SOC	A0
#define GPIO_BTN_LEFT	D1
#define GPIO_BTN_RIGHT	D2

// システムモード
typedef enum {
	MODE_NUM_PRE=0, MODE_NUM, 
	MODE_GRAPH_PRE, MODE_GRAPH, 
	MODE_LOG_PRE, MODE_LOG,
	MODE_CLOUD_PRE, MODE_CLOUD,
	MODE_CALIBRATE_PRE, MODE_CALIBRATE
} SYSTEM_MODE;

// グラフレンジ
typedef struct {
	char *name;
	char *max_name;
	char *half_name;
	uint16_t max_size;
	uint16_t half_size;
} RANGE_INFO;

// ボタン関係
extern volatile BTN_STATUS leftBtnStatus, rightBtnStatus;
