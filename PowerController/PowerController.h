/*
 * イベント監視用変数型定義
 */
typedef struct { // 監視するイベントを格納する変数型定義
  boolean keepAlive;             // keep alive信号が上がったか否か
  boolean highTemperature;       // CPU高温警告信号が上がったか否か
  int envTemperature;            // 環境温度がどの範囲に収まっているか
  int commandInput;              // コマンドラインで入力された文字の解析結果
  char commandInputChar;         // コマンドラインに入力された文字そのもの
  boolean timerFire;             // CPU高温時の冷却時間のタイマが発火したか否か
  boolean commandLineTimeOut;    // 管理者コンソールの自動ログオフのタイマが発火したか否か
  int powerPinCounter;           // 電源ボタンの監視用
} eventType;


