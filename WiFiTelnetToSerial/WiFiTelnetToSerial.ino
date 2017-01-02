
#define DEBUG
#define CONSOLE_MESSAGE
#define ESP
//#define SOFT_SERIAL

#define TIMER_THRESHOLD 60
#define MAX_PASS_RETRY 3

#define MAX_FILENAME_BUFF_SIZE 64
#define MAX_WIFI_SSID_SIZE 32
#define MAX_WIFI_PASS_SIZE MAX_WIFI_SSID_SIZE
#define MAX_USER_PASS_SIZE MAX_WIFI_SSID_SIZE

//how many clients should be able to telnet to this ESP8266
//#define MAX_SRV_CLIENTS 1

#define SD_CHIP_SELECT 5
#define LOGIN_PASSWD_FILE "pass.txt"
#define WIFI_SSID_FILE "ssid.txt"
#define WIFI_PASS_FILE "wifipass.txt"

#define SOFT_SERIAL_RX_PIN 16
#define SOFT_SERIAL_TX_PIN 4

/*
  状態遷移
    初期状態    STATE_START          0
    ログイン    STATE_LOGIN          1
    コマンド    STATE_COMMAND        2
    中継        STATE_RELAY          3
 */
#define STATE_NONE    0
#define STATE_START   1
#define STATE_LOGIN   2
#define STATE_COMMAND 3
#define STATE_RELAY   4
/*
   管理者用コマンド
    COMMAND_OPEN           'O'     : シリアルをオープン
    COMMAND_HELP           'H'|'?' : HELP             (コマンドの説明の出力)
    COMMAND_EXIT           'Q'     : Quit  管理者モードを抜ける
    COMMAND_REBOOT         'R'     : リブート
    COMMAND_ESCAPE         '^A'    : シリアルをクローズ
    COMMAND_UNKNOWN        コマンドに対応しない文字の入力時
    COMMAND_NONE           入力なし
*/
#define COMMAND_NONE          0
#define COMMAND_UNKNOWN       1
#define COMMAND_OPEN          2
#define COMMAND_HELP          3
#define COMMAND_EXIT          4
#define COMMAND_ESCAPE        5
#define COMMAND_REBOOT        6

#include <SPI.h>
#include <SD.h>
#include <ESP8266WiFi.h>

#ifdef SOFT_SERIAL
#include <EspSoftwareSerial.h>
EspSoftwareSerial swSer(SOFT_SERIAL_RX_PIN,SOFT_SERIAL_TX_PIN, false, 256);  // Rx ,Tx, inverse_logic, buffSize
#endif /* SOFT_SERIAL */

char wifi_ssid[MAX_WIFI_SSID_SIZE];
char wifi_pass[MAX_WIFI_PASS_SIZE];
char passwd[MAX_USER_PASS_SIZE];
int currentState=COMMAND_NONE;

WiFiServer server(23);
//WiFiClient serverClients[MAX_SRV_CLIENTS];
WiFiClient serverClient;

String readFileEntry(String filename){
  char buff[MAX_FILENAME_BUFF_SIZE];
  filename.toCharArray(buff, MAX_FILENAME_BUFF_SIZE);
  File tmpFile=SD.open(buff);
  if (tmpFile) {
#ifdef DEBUG
    Serial.print("open file = ");
    Serial.println(filename);
#endif /* DEBUG */
    // read from the file until there's nothing else in it:
    while (tmpFile.available()) {
      String line = tmpFile.readStringUntil('\n');
      if ('#'!=line.charAt(0)) {
        tmpFile.close();
#ifdef DEBUG
        Serial.println(line);
#endif
        return line;
      }
    }
  } else {
#ifdef CONSOLE_MESSAGE
    // if the file didn't open, print an error:
    Serial.println("error opening file");
#endif
  }
  tmpFile.close();
  return "";
}

void initSdCard(){
#ifdef CONSOLE_MESSAGE
  Serial.print("Initializing SD card...");
#endif
  if (!SD.begin(SD_CHIP_SELECT)) {
#ifdef CONSOLE_MESSAGE
    Serial.println("initialization failed!");
#endif
    return;
  }
#ifdef CONSOLE_MESSAGE
  Serial.println("initialization done.");
#endif
}

void readConfigFiles(){
  readFileEntry(LOGIN_PASSWD_FILE).toCharArray(passwd,MAX_USER_PASS_SIZE);
  for (int i=0 ; i< MAX_USER_PASS_SIZE;i++){
    if ((passwd[i]<32) || (passwd[i]>126)) {
      passwd[i]=0;
    }
  }
  readFileEntry(WIFI_SSID_FILE).toCharArray(wifi_ssid,MAX_WIFI_SSID_SIZE);
  for (int i=0 ; i< MAX_WIFI_SSID_SIZE;i++){
    if ((wifi_ssid[i]<32) || (wifi_ssid[i]>126)) {
      wifi_ssid[i]=0;
    }
  }
  readFileEntry(WIFI_PASS_FILE).toCharArray(wifi_pass,MAX_WIFI_PASS_SIZE);
  for (int i=0 ; i< MAX_WIFI_PASS_SIZE;i++){
    if ((wifi_pass[i]<32) || (wifi_pass[i]>126)) {
      wifi_pass[i]=0;
    }
  }
}

void printCommandHelp() {
  serverClient.println("Command mode");
  serverClient.println("\tO : open serial port");
  serverClient.println("\tH|? : Help");
  serverClient.println("\tQ : Quit");
  serverClient.println("\tR : reboot");
  serverClient.println("Console mode");
  serverClient.println("\t^A : close console port");
//  printMessage(COMMAND_HELP_MESSAGE,CONSOLE_SERIAL );
}

/*
  管理者インターフェースから入力されたコマンド(文字)の解析処理
  https://ja.wikipedia.org/wiki/ASCII#ASCII.E5.88.B6.E5.BE.A1.E6.96.87.E5.AD.97

   ASCIIコードの制御文字の代表例
   16進数         CS              意味
     00           ^@            ヌル文字
     01           ^A            ヘッディング開始
     03           ^C            テキスト終了
     04           ^D            伝送終了
     18           ^X            取り消し
     1A           ^Z            置換
     1B           ^[            エスケープ
 */
int commandLex(char input){
  switch (input) {
    case 'o': // シリアルポートで管理者がコマンドモードを利用する場合の合図
    case 'O':
      return COMMAND_OPEN;
    case 'H': // ヘルプ
    case 'h':
    case '?':
      return COMMAND_HELP;
    case 'Q': // 管理者コマンドモードからのログアウト
    case 'q':
      return COMMAND_EXIT;
    case 0x01 :
      return COMMAND_ESCAPE;
    case 'R' : // リブートコマンド
    case 'r' :
      return COMMAND_REBOOT;
  }
  return COMMAND_UNKNOWN; // 未定義の文字が入力された
}

void printLoginMessage(){
  serverClient.print("Please enter passwd to login : ");
}

boolean checkPasswd(char *loginPasswd){
/*
#ifdef DEBUG
  Serial.println("checkPasswd ");
  Serial.print("loginPasswd = ");
  Serial.println(loginPasswd);
  Serial.print("passwd = ");
  Serial.println(passwd);
  int test=strncmp(passwd,loginPasswd,MAX_PASS_LENGTH-1);
  Serial.print("strncmp(passwd,loginPasswd,MAX_PASS_LENGTH-1) = ");
  Serial.println(test);
#endif
*/
  if (0==strncmp(passwd,loginPasswd,MAX_USER_PASS_SIZE-1)) {
    return true;
  }
  return false;
}

boolean loginTest(){
  startCommandLineTimer();
#ifdef DEBUG
  Serial.println("");
  Serial.println("start loginTest()");
#endif /* DEBUG */
  //String loginPasswd = Serial.readStringUntil('\n');
  char loginPasswd[MAX_USER_PASS_SIZE];
#ifdef DEBUG
  //Serial.print("serverClient.available() = ");
  //Serial.println(serverClient.available());
#endif /* DEBUG */

  for (int i=0; i<MAX_PASS_RETRY;i++) {
    memset(loginPasswd,0,MAX_USER_PASS_SIZE);
    int j=0;
#ifdef DEBUG
  //Serial.print("serverClient.available() = ");
  //Serial.println(serverClient.available());
#endif /* DEBUG */
    for (;;){
      if (checkCommandLineTimer()){
        return false;
      }
#ifdef DEBUG
  //Serial.print("serverClient.available() = ");
  //Serial.println(serverClient.available());
#endif /* DEBUG */
      //delay(1);
#ifdef DEBUG
      //Serial.println("loginTest() No.1");
#endif /* DEBUG */
      if (serverClient.available()>0) {
        char incomingByte = serverClient.read();
        startCommandLineTimer();
        if ((j>0) && ((incomingByte==127)||(incomingByte==8))) {
          j--;
          loginPasswd[j]=0;
          continue;
        }
        if (((incomingByte==10)||(incomingByte==13)) && (j>0)) {
          serverClient.println("");
#ifdef DEBUG
          Serial.println("");
          Serial.print("passwd = ");
          Serial.println(passwd);
          Serial.print("input = ");
          Serial.println(loginPasswd);
#endif /* DEBUG */
          //パスワードを比較して結果を返す
          if (checkPasswd(loginPasswd)){
            return true;
          } else {
            serverClient.println("passwd error");
            serverClient.print("Please enter passswd again : ");
            break;
          }
        } else {
          if ((incomingByte < 128 ) && (incomingByte > 31)) {
            serverClient.print("*");
            //serverClient.print("*");
            loginPasswd[j]=incomingByte;
            j++;
          }
        }
        if (j==MAX_USER_PASS_SIZE) {
          serverClient.println("passwd too long");
          serverClient.print("Please enter passswd again : ");
#ifdef DEBUG
          Serial.println("");
          Serial.println("end loginTest() with false 1");
#endif /* DEBUG */
        }
      }
    }
  }
#ifdef DEBUG
  Serial.println("");
  Serial.println("end loginTest() with false 2");
#endif /* DEBUG */
  return false;
}

int stateStart(){
  printLoginMessage();
  return STATE_LOGIN;
}

void loginPenalty(){
  serverClient.println("too many failure");
  closeConnection();
  //delay(5000);
}

unsigned long commandLineTimer;                       // 管理者コンソールの自動ログオフのタイマ
void startCommandLineTimer(){ // コマンドラインのオートログオフ用タイマのスタート
  commandLineTimer=int(millis()/1000);
#ifdef DEBUG
  Serial.println("startCommandLineTimer()");
#endif /* DEBUG */
}

int stateLogin(){

  if (loginTest()) {
#ifdef DEBUG
    serverClient.println("login success");
#endif /* DEBUG */
    promptCommand();
    startCommandLineTimer();
    return STATE_COMMAND;
  }
  loginPenalty();
  return STATE_NONE;
}

void openConsole(){
  //Serial.println("This function is not yet implemented.");
}
void closeConsole(){
  //Serial.println("This function is not yet implemented.");
}
//
// 本体をリセットする関数
//
void reboot() {
#ifdef AVR
  asm volatile ("  jmp 0");
#endif
#ifdef ESP
  //Serial.println("reboot.");
  //ESP.restart();
  serverClient.println("This function is not yet implemented");
#endif
}

boolean checkCommandLineTimer(){ // コマンドラインのオートログオフ用タイマのチェック
  unsigned long diff=int(millis()/1000)-commandLineTimer;
#ifdef DEBUG
  //Serial.print("diff = ");
  //Serial.println(diff);
#endif /* DEBUG */
  if (TIMER_THRESHOLD < diff) {
#ifdef DEBUG
    //Serial.print("command line time out occur, diff = ");
    //Serial.println(diff);
#endif /* DEBUG */
    return true;
  } else {
    return false;
  }
}

void closeConnection(){
#ifdef DEBUG
  Serial.println("closeConnection()");
#endif /* DEBUG */
  serverClient.stop();
}

void promptCommand(){
  serverClient.print("command : ");
}
int stateCommand(){
  if (serverClient.available() > 0) {
    char incomingByte = serverClient.read();
    if ((incomingByte < 32)||(incomingByte==127)) {
      return STATE_COMMAND;
    }
    serverClient.println(incomingByte);
    startCommandLineTimer();
    switch(commandLex(incomingByte)){
      case COMMAND_OPEN:
        openConsole();
        return STATE_RELAY;
      case COMMAND_EXIT:
        closeConnection();
        return STATE_NONE;
      case COMMAND_REBOOT:
        serverClient.println("*** Reboot *** ");
        reboot();
        break;
      case COMMAND_ESCAPE:
      case COMMAND_UNKNOWN:
        serverClient.println("Error : Unknown Command");
      case COMMAND_HELP:
        printCommandHelp();
        promptCommand();
    }
  } else {
#ifdef DEBUG
    //Serial.println("command line timer check point");
#endif /* DEBUG */
    if (checkCommandLineTimer()){
      closeConnection();
      return STATE_NONE;
    }
  }
  return STATE_COMMAND;
}

int stateRelay(){
  if (checkCommandLineTimer()){
    closeConnection();
    return STATE_NONE;
  }
  //check clients for data
  if (serverClient && serverClient.connected()){
    if(serverClient.available()){
      //get data from the telnet client and push it to the UART
      while(serverClient.available()){
        char incomingByte = serverClient.read();
        startCommandLineTimer();
        switch(commandLex(incomingByte)){
          case COMMAND_ESCAPE:
            closeConsole();
            promptCommand();
            return STATE_COMMAND;
          default:
#ifdef SOFT_SERIAL
            swSer.write(incomingByte);
#else /* SOFT_SERIAL */
            Serial.write(incomingByte);
#endif /* SOFT_SERIAL */
        }
      }
    }
  }
#ifdef SOFT_SERIAL
  //check UART for data
  if(swSer.available()){
    size_t len = swSer.available();
    uint8_t sbuf[len];
    swSer.readBytes(sbuf, len);
    //push UART data to all connected telnet clients
    if (serverClient && serverClient.connected()) {
      serverClient.write(sbuf, len);
      delay(1);
    }
  }
#else /* SOFT_SERIAL */
  //check UART for data
  if(Serial.available()){
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    //push UART data to all connected telnet clients
    if (serverClient && serverClient.connected()) {
      serverClient.write(sbuf, len);
      delay(1);
    }
  }
#endif /* SOFT_SERIAL */
  return STATE_RELAY;
}

void checkClient(){
#if 0
  //check if there are any new clients
  if (server.hasClient()){
    if (!serverClient || !serverClient.connected()){
      if(serverClient) serverClient.stop();
      serverClient = server.available();
      currentState=STATE_START;
    }
    WiFiClient failClient = server.available();
    failClient.stop();
  }
#else
  if (server.hasClient()){
    serverClient.stop();
    serverClient = server.available();
    currentState=STATE_START;
  }
#endif
}

void setup() {
  Serial.begin(9600);
  initSdCard();
  readConfigFiles();
#ifdef DEBUG
  Serial.print("passwd = ");
  Serial.println(passwd);
  Serial.print("SSID = ");
  Serial.println(wifi_ssid);
  Serial.print("Wifi passwd = ");
  Serial.println(wifi_pass);
#endif /* DEBUG */
  WiFi.begin(wifi_ssid, wifi_pass);
#ifdef CONSOLE_MESSAGE
  Serial.print("\nConnecting to "); Serial.println(wifi_ssid);
#endif /* CONSOLE_MESSAGE */
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 100) {
    delay(500);
#ifdef CONSOLE_MESSAGE
    Serial.print(".");
#endif /* CONSOLE_MESSAGE */
  }
#ifdef CONSOLE_MESSAGE
  Serial.println("");
#endif /* CONSOLE_MESSAGE */
  if(i == 101){
#ifdef CONSOLE_MESSAGE
    Serial.print("Could not connect to "); Serial.println(wifi_ssid);
#endif /* CONSOLE_MESSAGE */
    while(1) delay(500);
  }
  //start UART and the server
  server.begin();
  server.setNoDelay(true);

#ifdef CONSOLE_MESSAGE
  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");
#endif /* CONSOLE_MESSAGE */
}


void loop() {
  checkClient();

  switch(currentState) {
    case STATE_NONE:
      delay(1);
      break;
    case STATE_START:
      currentState=stateStart();
      break;
    case STATE_LOGIN:
      currentState=stateLogin();
      break;
    case STATE_COMMAND:
      currentState=stateCommand();
      break;
    case STATE_RELAY:
      currentState=stateRelay();
  }
}
