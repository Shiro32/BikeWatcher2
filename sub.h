/*
  sub.h
  極小CO2測定器のサブルーチン集
*/

#ifndef _SUB_H_
#define _SUB_H_

// IoTクラウド（ambient）用設定
const int IOT_ID = 57196;
const char* IOT_WRITE_KEY = "d16d40711a7b4297";

void SelectCommMode( void );

void SetupWiFi( void );
void SetupOTA( void );
void SetupBatSOC( void );
float GetBattSoc( void );
bool WaitSec( uint32_t*, uint8_t );
void SendToCloud(void);
void SetupCloud( uint16_t );
void SendLineNotify(char *str);

const char* SSID1	= "Shiro-iPhone";
const char* PSWD1	= "hogehoge";

const char* SSID2	= "C330";
const char* PSWD2	= "hogehoge";

const char* SSID3	= "Buffalo-G-F600";
const char* PSWD3	= "bvjsci3wpcf4g";

const char* SSID4	= "&79<G]y";
const char* PSWD4	= "cE63f4?P";

#define LINE_TOKEN	"zG7RzSGz0lore6SNGqF2VrFcVX1r2OcKGezJq7moAVQ"
#define LINE_HOST	"notify-api.line.me"


typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT, ALIGN_TOP, ALIGN_BOTTOM } TEXT_ALIGN;

class OLED : public Adafruit_SSD1306 {
	public:
		using Adafruit_SSD1306::print;

		OLED( uint8_t w, uint8_t h, TwoWire *twi, int8_t rst_pin );
		void fill();
		void clear();
		void flush();
		
		void print( uint8_t, uint8_t, TEXT_ALIGN, uint8_t, const char*, bool );
		void print( uint8_t, uint8_t, TEXT_ALIGN, uint8_t, const char * );
		void print( uint8_t, uint8_t, uint8_t, const char*, bool );
		void print( uint8_t, uint8_t, uint8_t, const char* );

	private:
};


#endif

