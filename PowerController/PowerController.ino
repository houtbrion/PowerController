

/**************** カスタマイズする可能性がある定数定義 ****************/

#include "PowerControllerConfig.h"

/**************** カスタマイズ不要な定数定義 ****************/
/*
  状態定義
    種類                   状態名
    通常                 STATE_NORMAL
    電源OFF              STATE_OFF
    強制電源OFF          STATE_FOFF
    通常管理             STATE_NORMAL_CONTROL
    強制電源OFF管理      STATE_FOFF_CONTROL
    電源OFF管理          STATE_OFF_CONTROL
 */
#define STATE_NORMAL         0
#define STATE_OFF            1
#define STATE_FOFF           2
#define STATE_NORMAL_CONTROL 3
#define STATE_FOFF_CONTROL   4
#define STATE_OFF_CONTROL    5

/*
  管理用インターフェースの定義
    channdelID   physical link
       0             Serial
       1             network

*/
#define CONSOLE_SERIAL     0
#define CONSOLE_NETWORK    1

/*
   管理者用コマンド
     COMMAND_CONTROL_MODE  'L' : 管理者モードに移行(ログイン)
     COMMAND_SHUTDOWN      'S' :shutdown         (shutdownピンを1秒HIGH)
     COMMAND_POWER_ON      'U' :Power ON         (リレー用端子をLOW)
     COMMAND_POWER_DOWN    'D' :Power OFF        (リレー用端子をHIGH)
     COMMAND_INFO          'I' :Info            (各種の状態を返す)
         リレー端子の状態
         温度警告があったか否か
         (オプション) 周囲の温度
         直前の状態
         管理者モードから抜けた場合の行き先(状態)
    COMMAND_HELP          'H'|'?' : HELP             (コマンドの説明の出力)
    COMMAND_EXIT          'Q' : Quit  管理者モードを抜ける
    COMMAND_UNKNOWN       コマンドに対応しない文字の入力時
    COMMAND_NONE          入力なし
*/
#define COMMAND_NONE          0
#define COMMAND_CONTROL_MODE  1
#define COMMAND_SHUTDOWN      2
#define COMMAND_POWER_ON      3
#define COMMAND_POWER_DOWN    4
#define COMMAND_INFO          5
#define COMMAND_HELP          6
#define COMMAND_EXIT          7
#define COMMAND_UNKNOWN       8

/*
   環境温度の監視結果を示す定数
 */
#define ENV_TEMPERATURE_ERROR -1
#define ENV_TEMPERATURE_LOW    0
#define ENV_TEMPERATURE_MID    1
#define ENV_TEMPERATURE_HIGH   2

/**************** プログラム ****************/
/*
   ヘッダファイルのインクルード
 */
#include "DHT.h"
#include "Time.h"
#include "PowerController.h"
#ifdef LCD
#include <Wire.h>
#include "rgb_lcd.h"
#endif /* LCD */

#ifdef SOFT_SERIAL
#include <SoftwareSerial.h>
#define CONSOLE sSerial
#endif /* SOFT_SERIAL */

#ifdef SERIAL
#define CONSOLE Serial
#endif /* SERIAL */

#ifdef SERIAL1
#define CONSOLE Serial1
#endif /* SERIAL1 */

#ifdef SERIAL2
#define CONSOLE Serial2
#endif /* SERIAL2 */

#ifdef SERIAL3
#define CONSOLE Serial3
#endif /* SERIAL3 */

#ifdef BRIDGE
#include <Console.h>
#define CONSOLE Console
#endif /* BRIDGE */

#ifdef SOFT_WDT
#include <avr/wdt.h>
#endif /* SOFT_WDT */
/*
    各種変数型の定義とグローバル変数の定義&初期化
 */
 
eventType event;                               //イベントを格納する変数の定義と初期化
time_t timer;                                  // CPU高温時の冷却時間のタイマ
time_t commandLineTimer;                       // 管理者コンソールの自動ログオフのタイマ
volatile time_t keepAlive;                     // keep alive信号のタイマ変数

int currentState=-1;                           // システム(状態遷移機械)の状態を格納する変数
volatile boolean highTemperature=false;        // 過去に高温信号が上がったか否か
#ifndef NO_INTERRUPT
volatile boolean highTemperatureEvent=false;   // 高温信号の有無の一時格納用 (イベント監視の関数は常時初期化されるため)
volatile boolean keepAliveFlag=false;
#endif /* NO_INTERRUPT */

time_t infoTimer;

DHT dht;

#ifdef SOFT_SERIAL
SoftwareSerial sSerial(SOFT_SERIAL_RX, SOFT_SERIAL_TX); // RX, TX
#endif


#ifdef LCD
rgb_lcd lcd;
#endif /* LCD */
#ifdef LCD
void lcd_normal(){
  lcd.setRGB(255,255,255); // 白
}
void lcd_info(){
  lcd.setRGB(10,10,200); //  青
}
void lcd_causion(){
  lcd.setRGB(150,150,0); // 黄色
}
void lcd_allert(){
  lcd.setRGB(255,0,100); // 赤
}
#endif /* LCD */


#ifdef DEBUG
void dumpEvent(){
  Serial.print(F("keep alive = "));
  Serial.print((event.keepAlive ? "true" : "false"));
  Serial.print(F(" , highTemperature = "));
  Serial.print((event.highTemperature ? "true" : "false"));
  Serial.print(F(" , timerFire = "));
  Serial.print((event.timerFire ? "true" : "false"));
  Serial.print(F(" , commandLineTimeOut = "));
  Serial.println((event.commandLineTimeOut ? "true" : "false"));
  Serial.print(F("envTemperature = "));
  Serial.print(event.envTemperature);
  Serial.print(F(" , commandInput = "));
  Serial.print(event.commandInput);
  Serial.print(F(" , commandInputChar="));
  Serial.println(event.commandInputChar);
  Serial.print(F("current state = "));
  switch (currentState) {
    case STATE_NORMAL:
      Serial.println(F("STATE_NORMAL"));
      break;
    case STATE_OFF:
      Serial.println(F("STATE_OFF"));
      break;
    case STATE_FOFF:
      Serial.println(F("STATE_FOFF"));
      break;
    case STATE_NORMAL_CONTROL:
      Serial.println(F("STATE_NORMAL_CONTROL"));
      break;
    case STATE_FOFF_CONTROL:
      Serial.println(F("STATE_FOFF_CONTROL"));
      break;
    case STATE_OFF_CONTROL:
      Serial.println(F("STATE_OFF_CONTROL"));
  }
#ifdef DHT_WAIT
  delay(dht.getMinimumSamplingPeriod());
#endif /* DHT_WAIT */

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.getHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.getTemperature();

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F(" %\t"));
  Serial.print(F("Temperature: "));
  Serial.print(t);
  Serial.print(F(" *C "));
  Serial.print(F("now = "));
  Serial.print(now());
  Serial.print(F(" , timer="));
  Serial.print(timer);
  Serial.print(F(" , commandLineTimer = "));
  Serial.print(commandLineTimer);
  Serial.print(F(" , keepAlive = "));
  Serial.println(keepAlive);
}
#endif /* DEBUG */


#ifndef NO_INTERRUPT
/*
 * 割り込み処理の定義
 */
void keepAliveINT(){          // keep alive信号の割り込み処理
  keepAliveFlag=true;
}

void highTemperatureINT() {   // CPU高温信号が割り込み処理
  highTemperature=true;       // CPU高温警告が過去に上がったか否か(冷却処理とコマンドラインでのINFOコマンドのため) 冷却時間が過ぎるまで消さない
  highTemperatureEvent=true;  // CPU高温警告が上がったか否かの一時格納
}
#endif /* NO_INTERRUPT */

/*
 * 管理用インターフェースの文字入出力の仮想化
 */
/*
void printMessage(char *buff, int channelID) {  // 管理者用インターフェースをネットワークに対応させるための準備(ネットワークは未対応)
  switch(channelID) {
    case CONSOLE_SERIAL :
      Serial.print(buff);
      break;
    case CONSOLE_NETWORK :
      break;
  }
}
*/
/*
 * システムログの出力
 */
/*
void printLog(char *buff){  // 将来ログをファイルに保存する可能性があるための準備(未対応)
  Serial.print(buff);
}
*/
/*
 * 各種タイマのスタート処理
 */
void startTimer(){ // 高温時等の電源On/Offの待ち時間用タイマのスタート
  timer=now();
}
void startCommandLineTimer(){ // コマンドラインのオートログオフ用タイマのスタート
  commandLineTimer=now();
}

/*
 * 管理者インターフェースから入力されたコマンド(文字)の解析処理
 */
int commandLex(char input){
  switch (input) {
    case 'L': // シリアルポートで管理者がコマンドモードを利用する場合の合図
    case 'l':
      return COMMAND_CONTROL_MODE;
    case 'S': // Piをシャットダウンするコマンド
    case 's':
      return COMMAND_SHUTDOWN;
    case 'U': // 強制電源ON
    case 'u':
      return COMMAND_POWER_ON;
    case 'D': // 強制電源OFF
    case 'd':
      return COMMAND_POWER_DOWN;
    case 'I': // 現在状態の観測と表示
    case 'i':
      return COMMAND_INFO;
    case 'H': // ヘルプ
    case 'h':
    case '?':
      return COMMAND_HELP;
    case 'Q': // 管理者コマンドモードからのログアウト
    case 'q':
      return COMMAND_EXIT;
  }
  return COMMAND_UNKNOWN; // 未定義の文字が入力された
}

#ifdef LCD
void printStateLCD(float humidity,float temperature,int currentState){
  char buff[20];
  lcd.clear();
  if (isnan(humidity) || isnan(temperature)) {
    lcd.print(F("Sensor error."));
  } else {
    sprintf(buff,"T = %d ,H = %d", int(temperature), int(humidity));
    lcd.print(buff);
  }
  switch(currentState) {
    case STATE_NORMAL:
    case STATE_NORMAL_CONTROL:
      lcd.setCursor(0, 1);
      lcd.print(F("State=NORMAL"));
      break;
    case STATE_OFF:
    case STATE_OFF_CONTROL:
      lcd.setCursor(0, 1);
      lcd.print(F("State=OFF"));
      break;
    case STATE_FOFF:
    case STATE_FOFF_CONTROL:
      lcd.setCursor(0, 1);
      lcd.print(F("State=FOFF"));
  }
}
#endif /* LCD */

/*
 * イベント監視
 */
void checkEvent(){
  if (LOW==digitalRead(POWER_PIN)) {
    event.powerPinCounter=0;
  } else {
    event.powerPinCounter++;
  }
#ifdef DEBUG_POWER_PIN
  Serial.print("powerPinCounter = ");
  Serial.println(event.powerPinCounter);
#endif /* DEBUG_POWER_PIN */
  // Raspberry Piのkeep alive信号が一定時間以内に観測できたか否かの判定
#ifdef NO_INTERRUPT
  if (HIGH==digitalRead(KEEP_ALIVE_PIN)) {
    keepAlive=now();            // keep alive信号が上がった時間の格納
  }
#else /* NO_INTERRUPT */
  if (keepAliveFlag==true) {
    keepAlive=now();            // keep alive信号が上がった時間の格納
    keepAliveFlag=false;
  }
#endif /* NO_INTERRUPT */
  if ((keepAlive+KEEP_ALIVE_THRESHOLD) < now()) { // 前回の観測時間に制限時間を加えたものが現在時刻より未来になっていない場合は，タイムアウト (エラー発生)
    event.keepAlive=true;
  } else {                                        // タイムアウトになっていない(制限時間内)の場合
    event.keepAlive=false;
  }
#ifdef NO_INTERRUPT
  if (HIGH==digitalRead(HIGH_TEMP_PIN)) {
    event.highTemperature=true;
  }
#else /* NO_INTERRUPT */
  event.highTemperature=highTemperatureEvent;     // 割り込みで立てられたフラグの値を書き写す
  highTemperatureEvent=false;                     // 割り込みで立てられたフラグの値を消す
#endif /* NO_INTERRUPT */

#ifdef DHT_WAIT
  delay(dht.getMinimumSamplingPeriod());
#endif /* DHT_WAIT */
  float humidity = dht.getHumidity();            // 湿度の読み取り
  float temperature = dht.getTemperature();      // 温度の読み取り
  // 温度と湿度の読み取りに成功したか否かの判定
  if (isnan(humidity) || isnan(temperature)) {
    // どちらかが失敗している
    event.envTemperature=ENV_TEMPERATURE_ERROR;
  } else {
    // 両方の読み取りに成功した場合は，温度を区分け
    if (temperature < HI_LOW_THRESHOLD) {                  // 温度 『低』
      event.envTemperature=ENV_TEMPERATURE_LOW;
    } else if (temperature < HI_HIGH_THRESHOLD) {          // 温度 『中』
      event.envTemperature=ENV_TEMPERATURE_MID;
    } else {                                               // 温度 『高』
      event.envTemperature=ENV_TEMPERATURE_HIGH;
    }
  }
  printStateLCD(humidity,temperature,currentState);
  if (CONSOLE.available() > 0) {                  // シリアルポートへの入力に対する処理
    // シリアルポートに文字入力がある場合
    // 入力された文字がどのコマンドに対応するか解析(lex)し，生文字を保存して返す．
    char incomingByte = 0;
    // read the incoming byte:
    incomingByte = CONSOLE.read();
    if ((incomingByte>31) && (incomingByte<127)){
      startCommandLineTimer();                      // コマンドが何か入力された場合は，タイムアウト処理のためのタイマを走らせる．
      event.commandInput = commandLex(incomingByte);
      event.commandInputChar = incomingByte;
    }
  } else {
    // シリアルポートに文字入力がない場合は，『なし』で返答
    event.commandInput=COMMAND_NONE;
    event.commandInputChar=(char)0;
  }
  if ((timer!=0) && ((timer+HIGH_TEMPERATURE_WAIT_DURATION)<now())) { // CPU高温に対応するための，冷却時間が過ぎたか否かの判定
    event.timerFire=true;
    timer=0;
  } else {
    event.timerFire=false;
  }
  if ((commandLineTimer+COMMAND_LINE_WAIT_DURATION)< now()) { // コマンドラインの自動ログアウトの制限時間を超過しているか否かの判定
    event.commandLineTimeOut=true;
  } else {
    event.commandLineTimeOut=false;
  }
}

/*
     プリントinfo : 管理者コマンドモードで，Infoコマンドを入力された際の処理
       現在の『運転』状態の表示
       Raspberry PiのCPU高温警告が上がったための冷却待ち時間中か否か
       環境の温度・湿度の測定と体感温度の計算
 */
void printInfo(int state) {
  //printMessage("Current status: ",CONSOLE_SERIAL ); // 現在状態表示のヘッダ部分
  CONSOLE.print("Current status: ");
  switch (state) {                                    // 現在状態の表示 (管理者モードは普通の運転状態とマージ)
    case STATE_NORMAL:
    case STATE_NORMAL_CONTROL:
      //printMessage("Normal\n",CONSOLE_SERIAL);
      CONSOLE.println("Normal");
      break;
    case STATE_OFF:
    case STATE_OFF_CONTROL:
      //printMessage("Power off\n",CONSOLE_SERIAL);
      CONSOLE.println("Power off");
      break;
    case STATE_FOFF:
    case STATE_FOFF_CONTROL:
      //printMessage("Force power off\n",CONSOLE_SERIAL);
      CONSOLE.println("Force power off");
      break;
  }
  if (highTemperature) {                               // Raspberry PiからCPU温度警告による対応処理中か否かの表示
    //printMessage("Raspberry Pi CPU temperature alart : YES\n",CONSOLE_SERIAL);
    CONSOLE.println("Raspberry Pi CPU temperature alart : YES");
  } else {
    //printMessage("Raspberry Pi CPU temperature alart : NO\n",CONSOLE_SERIAL);
    CONSOLE.println("Raspberry Pi CPU temperature alart : NO");
  }
  // 環境の温・湿度を測定し，体感温度を計算する
  char buff[128];
#ifdef DHT_WAIT
  delay(dht.getMinimumSamplingPeriod());
#endif /* DHT_WAIT */
  float h = dht.getHumidity();
  float t = dht.getTemperature();
  //sprintf(buff,"Environment temperature = %d , humidity = %d\n",(int)t,(int)h);
  //printMessage(buff,CONSOLE_SERIAL);
  CONSOLE.print("Environment temperature = ");
  CONSOLE.print(int(t));
  CONSOLE.print(" , humidity = ");
  CONSOLE.println(int(h));
}

/*
 * INFO以外の各コマンドの処理と下請け関数
 */
/*
    電源用リレーの制御
      引数flag == true  : Power ON         (リレー用端子をLOW)
      引数flag == false : Power OFF        (リレー用端子をHIGH)
 */

void relayControl(boolean flag){
  if (flag){
    digitalWrite(RELAY_PIN,LOW);   // 通電開始
  } else {
    digitalWrite(RELAY_PIN,HIGH);  // 通電終了
  }
}

/*
   shutdownコマンドの処理
     Piと接続してるshutdownピンをSHUTDOWN_PIN_DURATION(秒)間HIGHにする
     shutdownピンをLOWにする
     SHUTDOWN_WAIT_DURATION(秒)待つ
 */
void forcePiShutdown(){
#ifdef LCD
  lcd.clear();
  lcd.print(F("Shutdown signal"));
  lcd.setCursor(0, 1);
#endif /* LCD */
  //printMessage("Start shutdown procedure of raspberry Pi.\n",CONSOLE_SERIAL);
  //printMessage("Sending shutdown signal to Pi.",CONSOLE_SERIAL);
  CONSOLE.println("Start shutdown procedure of raspberry Pi.");
  CONSOLE.print("Sending shutdown signal to Pi.");
  digitalWrite(SHUTDOWN_PIN,HIGH);            // Raspberry Piにshutdown指示するピンをHIGH
  int i;
  for (i=0;i<SHUTDOWN_PIN_DURATION;i++) {     // SHUTDOWN_PIN_DURATION(秒)だけ待機
#ifdef LCD
    lcd.print(F("."));
#endif /* LCD */
    //printMessage(".",CONSOLE_SERIAL);
    CONSOLE.print(".");
    delay(1000);
  }
#ifdef LCD
  lcd.print(F("done"));
#endif /* LCD */
  //printMessage("done\n",CONSOLE_SERIAL);
  CONSOLE.println("done.");
  digitalWrite(SHUTDOWN_PIN,LOW);             // Raspberry Piにshutdown指示するピンをLOW (Raspberry Piは信号に気づいてshutdownしているはず)
#ifdef LCD
  lcd.clear();
  lcd.print(F("Waiting for Pi"));
  lcd.setCursor(0, 1);
#endif /* LCD */
  //printMessage("Waiting for Pi.",CONSOLE_SERIAL);
  CONSOLE.print("Waiting for Pi.");
  for (i=0;i<SHUTDOWN_WAIT_DURATION;i++) {    // Raspberry Piのshutdown処理が終わるのを待つ (信号はないので，とにかく一定時間待つ)
#ifdef LCD
    lcd.print(F("."));
#endif /* LCD */
    //printMessage(".",CONSOLE_SERIAL);
    CONSOLE.print(".");
    delay(1000);
  }
#ifdef LCD
  lcd.print(F("done"));
#endif /* LCD */
  //printMessage("done\n",CONSOLE_SERIAL);
  CONSOLE.println("done");
}

/*
   reboot
     shutdownして電源をOFF，一定時間待って，電源ON
 */
void reboot() {
  int i;
  forcePiShutdown();
#ifdef LCD
  lcd.clear();
  lcd.print(F("Power OFF"));
  lcd.setCursor(0, 1);
#endif /* LCD */
  //printMessage("Power OFF",CONSOLE_SERIAL);
  CONSOLE.print("Power OFF");
  relayControl(false);              // 電源OFF
  for (i=0;i<POWER_ON_WAIT;i++) {   // 一定時間待機
    //printMessage(".",CONSOLE_SERIAL);
    CONSOLE.print(".");
#ifdef LCD
    lcd.print(F("."));
#endif /* LCD */
    delay(1000);
  }
  //printMessage("done\n",CONSOLE_SERIAL);
  CONSOLE.println("done");
#ifdef LCD
  lcd.print(F("done"));
#endif /* LCD */
  //printMessage("Power ON\n",CONSOLE_SERIAL);
  CONSOLE.println("Power ON");
#ifdef LCD
  lcd.clear();
  lcd.print(F("Power ON"));
#endif /* LCD */
  relayControl(true);               // 電源ON
}

/*
 HELP             (コマンドの説明の出力)
  S:shutdown         (shutdownピンを1秒HIGH)
  U:Power ON         (リレー用端子をLOW)
  D:Power OFF        (リレー用端子をHIGH)
  I:Info            (各種の状態を返す)
  H|? : HELP             (コマンドの説明の出力)
  Q : Quit  管理者モードを抜ける
*/

#define COMMAND_HELP_MESSAGE "S : Shutdown\nU : power Up\nD : power Down\nI : Info\nH|? : Help\nQ : Quit\n"
void printCommandHelp() {
  //printMessage(COMMAND_HELP_MESSAGE,CONSOLE_SERIAL );
  CONSOLE.println("S : Shutdown");
  CONSOLE.println("U : power Up");
  CONSOLE.println("D : power Down");
  CONSOLE.println("I : Info");
  CONSOLE.println("H|? : Help");
  CONSOLE.println("Q : Quit");
}

/*
   ログインコマンドエラー通知
 */
#define LOGIN_COMMAND_ERROR_MESSAGE "L : Login to control command line mode\n"
void printControlCommandError(){
  //printMessage(LOGIN_COMMAND_ERROR_MESSAGE,CONSOLE_SERIAL);
  CONSOLE.println("L : Login to control command line mode");
}

/*
   コマンドにない文字を入力した場合のエラー通知
 */
void printInputCommand(char input){
  //char buff[64];
  //sprintf(buff,"Error : you input unknown character '%c'.\n",input);
  //printMessage(buff,CONSOLE_SERIAL);
  CONSOLE.print("Error : you input unknown character '");
  CONSOLE.print(input);
  CONSOLE.println("'.");
}

/*
   エラー処理エントリ関数
      入力した文字が有効な文字か否かは，管理者モードにログイン前かログイン後で変化するため
*/
void printCommandErrorMessage(int state, char input){
  printInputCommand(input);
  switch(state) {
    case 0 : // 管理者モードでの不正文字入力
      printCommandHelp();
      break;
    case 1 : // 通常時にログイン以外の文字を入れた
      printControlCommandError();
      break;
  }
}

/*
 "通常 (STATE_NORMAL) "状態から他の状態に遷移する際の判定と遷移の際の処理
   【処理がある遷移】
    (1) keep alive 警告がある場合は，rebootしてkeep aliveを初期化して元に戻る
    (2) 高温信号があったら，shutdown+電源OFFし，タイマを仕掛けて電源OFF状態に遷移
    (3) 体感温度が一定以上の場合は，shutdown+電源OFFしてタイマを仕掛けて電源OFF状態に遷移
    (4) 電源ボタンが一定時間以上押されていたら，shutdown+電源OFFし，強制電源OFF状態に遷移
   【処理がない遷移】
    (5) 管理者状態への遷移要求があったら，通情管理者状態(STATE_NORMAL_CONTROL)に遷移
    (6) 想定外の文字列入力には，管理者モードへの入力説明を表示してもとに戻る
 */
int transition_normal(){
  if (event.powerPinCounter > POWER_PIN_THRESHOLD) { // 電源ボタンを一定時間押した場合 (4)
#ifdef LCD_COLOR
    lcd_causion();
#endif /* LCD_COLOR */
    forcePiShutdown();
    //printMessage("Power OFF",CONSOLE_SERIAL);
    CONSOLE.print("Power OFF");
#ifdef LCD
    lcd.clear();
    lcd.print(F("Power OFF"));
#endif /* LCD */
    relayControl(false);              // 電源OFF
    event.powerPinCounter=0;
    return STATE_FOFF;
  }
  if (event.keepAlive) {       // Raspberry Piがkeep alive信号を一定時間以上出さなかった． (1)
    // リブートして，通常状態に戻る
#ifdef LCD_COLOR
    lcd_allert();
#endif /* LCD_COLOR */
    reboot();
    keepAlive=now();
    infoTimer=now();
#ifdef LCD_COLOR
    lcd_normal();
#endif /* LCD_COLOR */
    return STATE_NORMAL;
  }
  if ((event.highTemperature)||(event.envTemperature==ENV_TEMPERATURE_HIGH)) {  // 環境温度か，PiのCPU温度が高い(2)か(3)
    // shutdownして，一定時間放置するためのタイマを走らせる．
#ifdef LCD_COLOR
    lcd_allert();
#endif /* LCD_COLOR */
    forcePiShutdown();
    //printMessage("Power OFF",CONSOLE_SERIAL);
    CONSOLE.print("Power OFF");
#ifdef LCD
    lcd.clear();/* noro */
    lcd.print(F("Power OFF"));
    lcd.setCursor(0, 1);
#endif /* LCD */
    relayControl(false);              // 電源OFF
    int i;
    for (i=0;i<POWER_ON_WAIT;i++) {   // 一定時間待機
      //printMessage(".",CONSOLE_SERIAL);
      CONSOLE.print(".");
#ifdef LCD
      lcd.print(F("."));
#endif /* LCD */
      delay(1000);
    }
    //printMessage("done\n",CONSOLE_SERIAL);
    CONSOLE.println("done");
#ifdef LCD
    lcd.print(F("done"));
#endif /* LCD */
    relayControl(false);
    startTimer();
    return STATE_OFF;
  }
  if (event.commandInput==COMMAND_CONTROL_MODE) {  // 管理者がログインした(5)
    // 管理者との対話処理(状態)に遷移
    //printMessage("Entering command line mode.\n",CONSOLE_SERIAL);
    CONSOLE.println("Entering command line mode.");
    printCommandHelp();
    return STATE_NORMAL_CONTROL;
  } else if (event.commandInput!=COMMAND_NONE) {   // コマンドとして意味がない文字が入力された (6)
    printCommandErrorMessage(1, event.commandInputChar); // エラーの表示
  }
  if ((infoTimer+INFO_INTERVAL) < now()){
    infoTimer=now();
    printInfo(STATE_NORMAL);
  }
#ifdef LCD_COLOR
  lcd_normal();
#endif /* LCD_COLOR */
  return STATE_NORMAL; //特に何もなければ，元の状態に戻る
}
/*
   "電源OFF"状態
   【処理がある遷移】
    (1) タイマが発火していれば，高温ログ(highTemperature)を消し，さらに，環境温度が一定以下なら，電源ONして，"通常"状態に遷移
    (2) タイマが動作していない(タイマ値が0)かつ環境温度が一定以下なら，電源ONして，"通常"状態に遷移
    (3) 電源ボタンが一定時間以上押されていたら，電源ONし，通常状態に遷移
   【処理がない遷移】
    (4) タイマ計時中(タイマが発火しておらず，タイマ値が0でない)なら元に戻る
    (5) 管理者モードへの遷移要求(文字入力)があったら，電源OFF管理状態に遷移
    (6) 想定外の文字列入力には，管理者モードへの入力説明を表示してもとに戻る
 */
int transition_off(){
  if (event.powerPinCounter > POWER_PIN_THRESHOLD) { // 電源ボタンを一定時間押した場合 (3)
      //printMessage("Power ON\n",CONSOLE_SERIAL);
      CONSOLE.println("Power ON");
#ifdef LCD
      lcd.clear();
      lcd.print(F("Power ON"));
#endif /* LCD */
      relayControl(true);                                  // 電源ON
      keepAlive=now();
      infoTimer=now();
      event.powerPinCounter=0;
#ifdef LCD_COLOR
      lcd_normal();
#endif /* LCD_COLOR */
      return STATE_NORMAL;
  }
  if (timer==0) {                                         // タイマが動作していない  (1)と(2) (4)
    if (event.timerFire) {                                // タイマが発火している場合はタイマのフラグは消す
      highTemperature=false;
    }
    if (event.envTemperature==ENV_TEMPERATURE_LOW) {       // 環境温度は低い
      //printMessage("Power ON\n",CONSOLE_SERIAL);
      CONSOLE.println("Power ON");
#ifdef LCD
      lcd.clear();
      lcd.print(F("Power ON"));
#endif /* LCD */
      relayControl(true);                                  // 電源ON
      keepAlive=now();
      infoTimer=now();
#ifdef LCD_COLOR
      lcd_normal();
#endif /* LCD_COLOR */
      return STATE_NORMAL;
    }
  }else if (event.commandInput!=COMMAND_NONE){             // コンソールからなにか入力あり
    if (COMMAND_CONTROL_MODE==event.commandInput) {        // 管理者モードへのログイン (5)
      //printMessage("Entering command line mode.\n",CONSOLE_SERIAL);
      CONSOLE.println("Entering command line mode.");
      printCommandHelp();
      return STATE_OFF_CONTROL;
    } else {                                               // 意味のない文字の入力(6)
      printCommandErrorMessage(1, event.commandInputChar);
    }
  }
  return STATE_OFF;
}
/*
  "強制電源OFF"状態
  【処理がない遷移】
   (1) 管理者モードへの遷移要求(文字入力)があったら，強制電源OFF管理状態に遷移
   (2) 想定外の文字列入力には，管理者モードへの入力説明を表示してもとに戻る
   (3) 電源ボタンが一定時間以上押されていたら，電源ONし，通常状態に遷移
 */
int transition_foff(){
  if (event.powerPinCounter > POWER_PIN_THRESHOLD) { // 電源ボタンを一定時間押した場合 (3)
      //printMessage("Power ON\n",CONSOLE_SERIAL);
      CONSOLE.println("Power ON");
#ifdef LCD
      lcd.clear();
      lcd.print(F("Power ON"));
#endif /* LCD */
      relayControl(true);                                  // 電源ON
      keepAlive=now();
      infoTimer=now();
      event.powerPinCounter=0;
#ifdef LCD_COLOR
      lcd_normal();
#endif /* LCD_COLOR */
      return STATE_NORMAL;
  }
  if (event.commandInput!=COMMAND_NONE){                      // コンソールから何か文字が入力された
    if (COMMAND_CONTROL_MODE==event.commandInput) {           // ログインだった
      //printMessage("Entering command line mode.\n",CONSOLE_SERIAL);
      CONSOLE.println("Entering command line mode.");
      printCommandHelp();
      return STATE_FOFF_CONTROL;
    } else {                                                  // 無意味な文字だった
      printCommandErrorMessage(1, event.commandInputChar);
    }
  }
  return STATE_FOFF;
}
/*
 * "管理"状態から他の状態に遷移する際の判定と遷移の際の処理 (管理状態は合計3種類存在する)
 *  (1) 強制電源OFF管理状態
 *  (2) 電源OFF管理状態
 *  (3) 通常管理状態
 */
/*
管理状態のコマンドと対応する処理の実行 : コマンドの読み替え等が必要ない場合の処理の実行 (共通処理)
                                                                                                  各状態で意味があるか否か
コマンド                                 内容                  対応する処理            通常管理       電源OFF管理      強制電源OFF管理
S:shutdown      COMMAND_SHUTDOWN   (shutdownピンを1秒HIGH) : forcePiShutdown()              ◯              ×                 ×
U:Power ON      COMMAND_POWER_ON   (リレー用端子をLOW)     : relayControl(true)             △              ◯                 ◯
D:Power OFF     COMMAND_POWER_DOWN (リレー用端子をHIGH)    : relayControl(false)            ◯              △                 △
I:Info          COMMAND_INFO       (各種の状態を返す)      : printInfo(現在の状態)          ◯              ◯                 ◯
H|? : HELP      COMMAND_HELP       (コマンドの説明の出力)  : printCommandHelp()             ◯              ◯                 ◯
Q : Quit        COMMAND_EXIT       管理者モードを抜ける                                     ◯              ◯                 ◯
*/
void exec_command_normally(int commandtype,int currentState) { /* noro */
  switch(commandtype) {
    case COMMAND_SHUTDOWN:
      forcePiShutdown();
      break;
    case COMMAND_POWER_ON:
#ifdef LCD
      lcd.clear();/* noro */
      lcd.print(F("Power ON"));
#endif /* LCD */
      relayControl(true);
      //printMessage("Power ON\n",CONSOLE_SERIAL);
      CONSOLE.println("Power ON");
#ifdef LCD_COLOR
      lcd_normal();
#endif /* LCD_COLOR */
      break;
    case COMMAND_POWER_DOWN:
#ifdef LCD_COLOR
      lcd_causion();
#endif /* LCD_COLOR */
#ifdef LCD
      lcd.clear();
      lcd.print(F("Power OFF"));
#endif /* LCD */
      relayControl(false);
      //printMessage("Power OFF\n",CONSOLE_SERIAL);
      CONSOLE.println("Power OFF");
      break;
    case COMMAND_INFO:
      printInfo(currentState);
      break;
    case COMMAND_HELP:
      printCommandHelp();
  }
}

/*
   強制電源OFF管理状態
   【処理がある遷移】
    (1) 強制電源ONコマンドが入力された && 環境温度がLOW -> 電源ONして，通常管理状態(STATE_NORMAL_CONTROL)に遷移
    (2) 強制電源ONコマンド以外のコマンドが入力された -> コマンドの内容を実行して元に戻る
    (3) 電源ボタンが一定時間以上押されていたら，電源ONし，通常管理状態(STATE_NORMAL_CONTROL)に遷移
   【処理がない遷移】
    (4) 入力がない & コマンドラインタイマが一定時間以上経過している -> 強制電源OFF状態(STATE_FOFF)に遷移
    (5) 強制電源ONコマンドが入力された && 環境温度がLOWではない -> 高温エラー表示で，元(STATE_FOFF_CONTROL)に戻る
    (6) exit コマンド(COMMAND_EXIT)が入力された -> 強制電源OFF状態(STATE_FOFF)に遷移
    (7) 知らない文字が入力された -> エラーを表示して元に戻る
 */
int transition_control_foff(){
  if (event.powerPinCounter > POWER_PIN_THRESHOLD) { // 電源ボタンを一定時間押した場合 (3)
    //電源ONして，通常管理状態(STATE_NORMAL_CONTROL)に遷移
#ifdef LCD
    lcd.clear();
    lcd.print(F("Power ON"));
#endif /* LCD */
    //printMessage("Power ON\n",CONSOLE_SERIAL);
    CONSOLE.println("Power ON");
    relayControl(true);
    keepAlive=now();
    event.powerPinCounter=0;
#ifdef LCD_COLOR
    lcd_normal();
#endif /* LCD_COLOR */
    return STATE_NORMAL_CONTROL;
  }
  if ((event.commandInput==COMMAND_NONE)&&event.commandLineTimeOut){ // 遷移(4)
    //printMessage("\nlogout.\n",CONSOLE_SERIAL);
    CONSOLE.println("");
    CONSOLE.println("logout.");
    commandLineTimer=0;
    keepAlive=now();
    return STATE_FOFF;
  }
  if (event.commandInput==COMMAND_POWER_ON) {
    if (event.envTemperature==ENV_TEMPERATURE_LOW) {                 // 遷移(1)
      //電源ONして，通常管理状態(STATE_NORMAL_CONTROL)に遷移
      //printMessage("Power ON\n",CONSOLE_SERIAL);
      CONSOLE.println("Power ON");
#ifdef LCD
      lcd.clear();
      lcd.print(F("Power ON"));
#endif /* LCD */
      relayControl(true);
      keepAlive=now();
#ifdef LCD_COLOR
      lcd_normal();
#endif /* LCD_COLOR */
      return STATE_NORMAL_CONTROL;
    } else {                                                         // 遷移(5) 
      //printMessage("Error : Enviroment temperature is too high.\n",CONSOLE_SERIAL);
      CONSOLE.println("Error : Enviroment temperature is too high.");
#ifdef LCD_COLOR
      lcd_allert();
#endif /* LCD_COLOR */
#ifdef LCD
      lcd.clear();/* noro */
      lcd.print(F("Temperature Error"));
#endif /* LCD */
      return STATE_FOFF_CONTROL;
    }
  }
  if (event.commandInput==COMMAND_EXIT) {                            // 遷移(6)
    return STATE_FOFF;
  }
  if (event.commandInput==COMMAND_UNKNOWN) {                         // 遷移(7)
    //printControlCommandError();
    printCommandErrorMessage(0, event.commandInputChar);
  }
  if (event.commandInput!=COMMAND_NONE) {                            // 遷移(2)
    exec_command_normally(event.commandInput,STATE_FOFF_CONTROL);
  }
  return STATE_FOFF_CONTROL;
}

/*
   電源OFF管理状態
   【処理がある遷移】
     (1) 強制電源ONコマンドが入力された && タイマは発火済み && 環境温度がLOW -> 電源ONして，通常管理状態(STATE_NORMAL_CONTROL)に遷移
     (2) 強制電源ONコマンド以外のコマンドが入力された -> コマンドの内容を実行して元に戻る
     (3) 電源ボタンが一定時間以上押されていたら，電源ONし，通常管理状態(STATE_NORMAL_CONTROL)に遷移
   【処理がない遷移】
     (4) 入力がない & コマンドラインタイマが一定時間以上経過している -> 電源OFF状態(STATE_OFF)に遷移
     (5) 強制電源ONコマンドが入力された && タイマは発火済み && 環境温度がLOW以外 -> 高温エラー表示で，元(STATE_OFF_CONTROL)に戻る
     (6) 強制電源ONコマンドが入力された && タイマは発火していない -> エラー表示で元(STATE_OFF_CONTROL)に戻る
     (7) exit コマンド(COMMAND_EXIT)が入力された -> 電源OFF状態(STATE_OFF)に遷移
     (8) 知らない文字が入力された -> エラーを表示して元に戻る
 */
int transition_control_off(){
  if (event.powerPinCounter > POWER_PIN_THRESHOLD) { // 電源ボタンを一定時間押した場合 (3)
    //電源ONして，通常管理状態(STATE_NORMAL_CONTROL)に遷移
    //printMessage("Power ON\n",CONSOLE_SERIAL);
    CONSOLE.println("Power ON");
#ifdef LCD
    lcd.clear();
    lcd.print(F("Power ON"));
#endif /* LCD */
    relayControl(true);
    keepAlive=now();
    event.powerPinCounter=0;
#ifdef LCD_COLOR
    lcd_normal();
#endif /* LCD_COLOR */
    return STATE_NORMAL_CONTROL;
  }
  if (timer==0) {
    if (event.timerFire) {
      highTemperature=false;
    }
  }
  if ((event.commandInput==COMMAND_NONE)&&event.commandLineTimeOut){   // 遷移(4)
    //printMessage("\nlogout.\n",CONSOLE_SERIAL);
    CONSOLE.println("");
    CONSOLE.println("logout.");
    commandLineTimer=0;
    keepAlive=now();
    return STATE_OFF;
  }
  if (event.commandInput==COMMAND_POWER_ON) {
    if (highTemperature) {                                              // 遷移(6)
      //printMessage("Error : Waiting for Pi cooling down.\n",CONSOLE_SERIAL);
      CONSOLE.println("Error : Waiting for Pi cooling down.");
#ifdef LCD_COLOR
      lcd_allert();
#endif /* LCD_COLOR */
#ifdef LCD
      lcd.clear();
      lcd.print(F("Temperature Error"));
#endif /* LCD */
      return STATE_OFF_CONTROL;
    } else {
      if (event.envTemperature==ENV_TEMPERATURE_LOW) {                  // 遷移(1)
        //printMessage("Power ON\n",CONSOLE_SERIAL);
        CONSOLE.println("Power ON");
#ifdef LCD
        lcd.clear();
        lcd.print(F("Power ON"));
#endif /* LCD */
        relayControl(true);
        keepAlive=now();
#ifdef LCD_COLOR
        lcd_normal();
#endif /* LCD_COLOR */
        return STATE_NORMAL_CONTROL;
      } else {                                                          // 遷移(5)
        //printMessage("Error : Enviroment temperature is too high.\n",CONSOLE_SERIAL);
        CONSOLE.println("Error : Enviroment temperature is too high.");
#ifdef LCD_COLOR
        lcd_allert();
#endif /* LCD_COLOR */
#ifdef LCD
        lcd.clear();
        lcd.print(F("Temperature Error"));
#endif /* LCD */
        return STATE_OFF_CONTROL;
      }
    }
  }
  if (event.commandInput==COMMAND_EXIT) {                               // 遷移(7)
    return STATE_OFF;
  }
  if (event.commandInput==COMMAND_UNKNOWN) {                            // 遷移(8)
    //printControlCommandError();
    printCommandErrorMessage(0, event.commandInputChar);
  }
  if (event.commandInput!=COMMAND_NONE) {                               // 遷移(2)
    exec_command_normally(event.commandInput,STATE_OFF_CONTROL);
  }
  return STATE_OFF_CONTROL;
}

/*
   通常管理状態
   【処理がある遷移】
    (1) 環境温度がHIGH || 高温信号あり -> 警告表示 , shutdown, 電源OFF,タイマスタートし，電源OFF管理状態(STATE_OFF_CONTROL)に遷移
    (2) shutdownコマンド -> shutdown, 電源OFFし，強制電源OFF管理状態(STATE_FOFF_CONTROL)に遷移
    (3) 電源OFFコマンド -> 警告表示，shutdown, 電源OFFし，強制電源OFF管理状態(STATE_FOFF_CONTROL)に遷移
    (4) その他のコマンド -> 通常の処理
    (5) 電源ボタンが一定時間以上押されていたら，shutdown, 電源OFFし，強制電源OFF管理状態(STATE_FOFF_CONTROL)に遷移
   【処理がない遷移】
    (6) 入力がない & コマンドラインタイマが一定時間以上経過している -> 通常状態(STATE_NORMAL)に遷移
    (7) 電源ONコマンド -> 警告表示のみ
    (8) exit コマンド(COMMAND_EXIT)が入力された -> keepAliveのタイマをリセットし，通常状態(STATE_NORMAL)に遷移
    (9) 知らない文字が入力された -> エラーを表示して元に戻る
 */
int transition_control_normal(){
  if (event.powerPinCounter > POWER_PIN_THRESHOLD) { // 電源ボタンを一定時間押した場合 (5)
#ifdef LCD_COLOR
    lcd_causion();
#endif /* LCD_COLOR */
    forcePiShutdown();
#ifdef LCD
    lcd.clear();
    lcd.print(F("Power OFF"));
#endif /* LCD */
    //printMessage("Power OFF",CONSOLE_SERIAL);
    CONSOLE.print("Power OFF");
    relayControl(false);
    return STATE_FOFF_CONTROL;
  }
  if ((event.commandInput==COMMAND_NONE)&&event.commandLineTimeOut){                        // 遷移(6)
    //printMessage("\nlogout.\n",CONSOLE_SERIAL);
    CONSOLE.println("");
    CONSOLE.println("logout.");
    commandLineTimer=0;
    keepAlive=now();
    infoTimer=now();
#ifdef LCD_COLOR
    lcd_normal();
#endif /* LCD_COLOR */
    return STATE_NORMAL;
  }
  if ((event.highTemperature)||(event.envTemperature==ENV_TEMPERATURE_HIGH)) {              // 遷移(1)
    if (event.highTemperature) {
#ifdef LCD_COLOR
      lcd_allert();
#endif /* LCD_COLOR */
      //printMessage("CPU temperature Alert.\n",CONSOLE_SERIAL);
      CONSOLE.println("CPU temperature Alert.");
#ifdef LCD
      lcd.clear();
      lcd.print(F("Temperature Error"));
#endif /* LCD */
    }
    if (event.envTemperature==ENV_TEMPERATURE_HIGH) {
#ifdef LCD_COLOR
      lcd_allert();
#endif /* LCD_COLOR */
      //printMessage("Environment temperature Alert.\n",CONSOLE_SERIAL);
      CONSOLE.println("Environment temperature Alert.");
#ifdef LCD
      lcd.clear();
      lcd.print(F("Temperature Error"));
#endif /* LCD */
    }
    forcePiShutdown();
    //printMessage("Power OFF\n",CONSOLE_SERIAL);
    CONSOLE.println("Power OFF");
#ifdef LCD
    lcd.clear();
    lcd.print(F("Power OFF"));
#endif /* LCD */
    relayControl(false);
    startTimer();
    return STATE_OFF_CONTROL;
  }
  if (event.commandInput==COMMAND_EXIT) {                                                   // 遷移(8)
    keepAlive=now();
    infoTimer=now();
#ifdef LCD_COLOR
    lcd_normal();
#endif /* LCD_COLOR */
    return STATE_NORMAL;
  }
  if ((event.commandInput==COMMAND_SHUTDOWN) || (event.commandInput==COMMAND_POWER_DOWN)) { // 遷移(2)と(3)
    if (event.commandInput==COMMAND_POWER_DOWN) {                                           // 遷移(3)のみ
      //printMessage("start shutdown procedure (Force power off is not permitted).\n",CONSOLE_SERIAL);
      CONSOLE.println("start shutdown procedure (Force power off is not permitted).");
    }
    forcePiShutdown();
    //printMessage("Power OFF\n",CONSOLE_SERIAL);
    CONSOLE.println("Power OFF");
#ifdef LCD
    lcd.clear();
    lcd.print(F("Power OFF"));
#endif /* LCD */
    relayControl(false);
    startTimer();
    return STATE_FOFF_CONTROL;
  }
  if (event.commandInput==COMMAND_POWER_ON) {                                               // 遷移(7)
    //printMessage("Power is already UP.\n",CONSOLE_SERIAL);
    CONSOLE.println("Power is already UP.");
#ifdef LCD
    lcd.clear();
    lcd.print(F("Already ON"));
#endif /* LCD */
    return STATE_NORMAL_CONTROL;
  }
  if (event.commandInput!=COMMAND_NONE) {                                                   // 遷移(4)
    exec_command_normally(event.commandInput,STATE_NORMAL_CONTROL);
  }
  if (event.commandInput==COMMAND_UNKNOWN) {                                                // 遷移(9)
    //printControlCommandError();
    printCommandErrorMessage(0, event.commandInputChar);
  }
  return STATE_NORMAL_CONTROL;
}

/*
   初期処理
 */
void setup() {
  Serial.begin(9600);
#ifdef SOFT_WDT
  wdt_enable(WDTO_8S);
#endif /* SOFT_WDT */
#ifdef DEBUG
  Serial.println("start");
#endif
#ifdef DEBUG
  Serial.println("relay pin setup");
#endif
  // Raspberry Piと接続するケーブルのデジタルI/Oのピンの動作定義
  pinMode(RELAY_PIN,     OUTPUT);
  relayControl(true);
#ifdef DEBUG
  Serial.println("done");
#endif
  //digitalWrite(RELAY_PIN,LOW);
#ifdef DEBUG
  Serial.println("shutdown pin setup");
#endif
  pinMode(SHUTDOWN_PIN,  OUTPUT);
  digitalWrite(SHUTDOWN_PIN,LOW);
#ifdef DEBUG
  Serial.println("done");
#endif
#ifdef DEBUG
  Serial.println("keep alive pin setup");
#endif
  pinMode(KEEP_ALIVE_PIN, INPUT);
  //digitalWrite(KEEP_ALIVE_PIN,LOW);
#ifdef DEBUG
  Serial.println("done");
#endif
  //pinMode(HIGH_TEMP_PIN,  OUTPUT);
  //digitalWrite(HIGH_TEMP_PIN,LOW);
  pinMode(HIGH_TEMP_PIN,  INPUT);
#ifdef DEBUG
  Serial.println("high temperature pin setup");
#endif
#ifdef DEBUG
  Serial.println("done");
#endif
#ifdef DEBUG
  Serial.println("power button pin setup");
#endif
  pinMode(POWER_PIN,      INPUT);
  //digitalWrite(POWER_PIN,LOW);
#ifdef DEBUG
  Serial.println("done");
#endif

#if defined(SOFT_SERIAL) || defined(SERIAL1) || defined(SERIAL2) || defined(SERIAL3)
  CONSOLE.begin(9600);
#endif /* SOFT_SERIAL || SERIAL1 || SERIAL2 || SERIAL3 */
#ifdef BRIDGE
  Bridge.begin();
  Console.begin();
#endif /* BRIDGE */
#ifdef LCD
  lcd.begin(16, 2);
#endif /* LCD */
#ifdef LCD_COLOR
  lcd_info();
#endif /* LCD_COLOR */
#ifdef LCD
  lcd.clear();
  lcd.print(F("Arduino Boot"));
#endif /* LCD */
  event.keepAlive=false;
  event.highTemperature=false;
  event.timerFire=false;
  event.commandInputChar=(char)0;
  //long hoge=millis();
  keepAlive=now();

  dht.setup(DHT_PIN);                    // 温度・湿度センサの動作開始
  //printLog("attach interupt to pins\n");
  CONSOLE.println("attach interupt to pins.");
#ifndef NO_INTERRUPT
  attachInterrupt(digitalPinToInterrupt(KEEP_ALIVE_PIN), keepAliveINT, RISING);               // Raspberry Piのkeep alive信号を監視するピンに割り込みを割当
  attachInterrupt(digitalPinToInterrupt(HIGH_TEMP_PIN), highTemperatureINT, RISING);          // Raspberry PiのCPU温度警告(高温)を監視するピンに割り込みを割当
#endif /* NO_INTERRUPT */
  //digitalWrite(HIGH_TEMP_PIN,LOW);
  //printLog("done.\n");
  CONSOLE.println("done.");
  currentState=STATE_NORMAL;             // システムの動作開始時は通常運用状態
  infoTimer=now();
#ifdef LCD_COLOR
  lcd_normal();
#endif /* LCD_COLOR */
}

/*
   本体処理 (メインループ) :  一定時間(LOOP_WAIT_TIME)間隔でイベントの有無を検査し，その結果を用いて遷移処理を行う
   
   状態名                 状態定数                          遷移処理
    通常                  STATE_NORMAL                     transition_control_normal()
    電源OFF               STATE_OFF                        transition_off()
    強制電源OFF           STATE_FOFF                       transition_foff()
    通常管理              STATE_NORMAL_CONTROL             transition_control_normal()
    強制電源OFF管理       STATE_FOFF_CONTROL               transition_control_foff()
    電源OFF管理           STATE_OFF_CONTROL                transition_control_off()
 */
void loop() {
  checkEvent();  // イベントの有無の確認処理
#if defined(DEBUG) && defined(DEBUG_EVENT)
  dumpEvent();
#endif /* DEBUG */
  switch (currentState) { // 状態遷移処理
    case STATE_NORMAL:
      currentState=transition_normal();
      break;
    case STATE_OFF:
      currentState=transition_off();
      break;
    case STATE_FOFF:
      currentState=transition_foff();
      break;
    case STATE_NORMAL_CONTROL:
      currentState=transition_control_normal();
      break;
    case STATE_FOFF_CONTROL:
      currentState=transition_control_foff();
      break;
    case STATE_OFF_CONTROL:
      currentState=transition_control_off();
  }
  delay(LOOP_WAIT_TIME); // 次のイベントチェックまでの待ち時間
#ifdef SOFT_WDT
  wdt_reset();
#endif /* SOFT_WDT */
}
