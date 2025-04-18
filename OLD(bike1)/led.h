//  ファイル：led.ino
//  目的：adafruitのled関係処理

const int kMatrix88Adr = 0x70;

void InitMatrix88( void );
void IndicateMatrix88( int now, int max );
void SetMatrix88( String s );
void ScrollMatrix88( void );
void TextMatrix88( String s );
void BitmapMatrix88( const uint8_t *bmp, bool blink );

void BlinkPowerLED( void );
void SendLineNotify(char *str);
void Talk( String s );
void CheckWifi( void );
void SetupOTA( void );
void CountSystemTickSec( void );
int SystemTickSec( void );
void IRAM_ATTR onShakeHandler( void );


