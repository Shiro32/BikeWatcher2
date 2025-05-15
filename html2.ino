/*
  ファイル：html.ino
  目的： webサーバ機能を実装して外部から操縦するためのルーチンたち

  履歴：
  2022/10/21 初版稼働

  /		ボタンの表示
  /off	警報停止

*/


const char* kHTML_FONT_SIZE	= "400%";


// void InitServer
// 外部からアクセスするためのWEB Serverの設定
void InitServer( void ) {
	server.on( "/", HandleRoot );
	server.on( "/off", HandleSystemOff );
	server.on( "/stop", HandleStopAlarm );
	server.on( "/start", HandleStart );
	server.onNotFound( HandleRoot );
	server.begin();
}

//------------------------------------------------------------------------------
// HandleOff
// ./offで呼び出され、警報を停止するだけ
void HandleStopAlarm( void ) {
	String html;

	gSystemMode = WAIT_SHORT;
	gVibrationDetected = false;	// 振動スイッチハンドラでTrueになり、ここでオフにする

	Serial.println("警報停止！！" );
	SendLineNotify( "警報を停止しました！" );
	//Talk( "ke'ihouwo teisi'simasita.", true );
	//BitmapMatrix88( kStopBmp, false );

	// HTMLのコード（自動遷移でrootに戻る）
	html = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>警報をとめる</title>
<meta http-equiv="Refresh" content="3;URL=../">
</head>
<body>
<p style="font-size:400%"; >応答あり。警報停止しました。</p>
</body>
</html>)";

	// HTMLを出力する
	server.send(200, "text/html", html);
}

//------------------------------------------------------------------------------
void HandleStart( void ) {
	String html;

	gSystemMode = WAIT_STD;	// これだけでシステム再起動できる
	//Talk( "kan'siwo kai'sisimasita.", true );
	SendLineNotify( "自転車の監視を開始しました！" );


	html = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>監視開始！</title>
<meta http-equiv="Refresh" content="3;URL=../">
</head>
<body>
<p style="font-size:400%"; >接続完了。盗難監視開始！</p>
</body>
</html>)";

	// HTMLを出力する
	server.send(200, "text/html", html);
}

//------------------------------------------------------------------------------
// システム停止
void HandleSystemOff( void ) {
	String html;

	gSystemMode = STOP;
	//Talk( "kan'siwo tei'sisi'masita", true );
	SendLineNotify( "自転車の監視を終了しました！" );


	html = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>監視開始！</title>
<meta http-equiv="Refresh" content="3;URL=../">
</head>
<body>
<p style="font-size:400%"; >応答あり。盗難監視を終了！</p>
</body>
</html>)";

	// HTMLを出力する
	server.send(200, "text/html", html);
}

//------------------------------------------------------------------------------
// HandleRoot
// ./で呼び出され、ボタンを設置する
void HandleRoot( void ) {
	String html;

	// HTMLの冒頭部分
	html = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>警報をとめる</title>
</head>
<body>
<p style="font-size:400%"; >【自転車盗難アラームシステム】</p>
<br><br>
<input type='button' value='始める' onclick='location.href="start"' style="width:100%; height:2em; padding:50px; font-size:1000%;" />
)";

	// 起動待ち状態でなければ、警報停止や終了ボタンが登場
	if( gSystemMode >= RESUME ) {
		html += R"(
<br><br><br>
<input type='button' value='警報停止' onclick='location.href="stop"' style="width:100%; height:2em; padding:50px; font-size:1000%;" />
<br><br><br>
<input type='button' value='やめる' onclick='location.href="off"' style="width:100%; height:2em; padding:50px; font-size:1000%;" />
		)";
	}

	// HTMLの末尾
	html += R"(
</body>
</html>)";

	// HTMLを出力する
	server.send(200, "text/html", html);
}
