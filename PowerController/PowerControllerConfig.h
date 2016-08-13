
/*
 * 利用する機能の定義
 */
//#define DEBUG
//#define DEBUG_EVENT
//#define DEBUG_POWER_PIN
#define DHT_WAIT
#define LCD
#define LCD_COLOR

//#define SERIAL
//#define SOFT_SERIAL
#define YUN

/*
 * 機種の定義
 */
#define UNO

/*

ピン配列の定義
 
 用途                           Raspberry Pi     UNO
 【割り込み信号 Pi -> Arduino】
 keep alive                         17            2 (黄)
 高温信号                           27            3 (緑)
 【Arduino -> Pi】
 shutdown                           22            7 (青)
 【温度センサ】
  DHTデータピン                                   4
 【電源制御】
  リレーのコントール端子                          5
  パワーボタン                                    6

*/
#ifdef UNO
#define KEEP_ALIVE_PIN  2
#define HIGH_TEMP_PIN   3
#define SHUTDOWN_PIN    7
#define DHT_PIN         4
#define RELAY_PIN       5
#define POWER_PIN       6
#endif /* UNO */

#ifdef SOFT_SERIAL
#define SOFT_SERIAL_RX  9
#define SOFT_SERIAL_TX  8
#endif /* SOFT_SERIAL */


/*
 * 各種定数の定義
 */
#define LOOP_WAIT_TIME    1000             // 1s

//イベント監視関連の定数
#define KEEP_ALIVE_THRESHOLD 30            // キープアライブがタイムアウトする時間間隔 30秒
#define HI_LOW_THRESHOLD     30            // 環境の温度のしきい値(下)
#define HI_HIGH_THRESHOLD    50            // 環境の温度のしきい値(上)
//#define HI_LOW_THRESHOLD     20            // 環境の体感温度のしきい値(下)
//#define HI_HIGH_THRESHOLD    27            // 環境の体感温度のしきい値(上)
#ifdef DEBUG
#define HIGH_TEMPERATURE_WAIT_DURATION 30 // 高温時の待ち時間 (30秒)
#else /* DEBUG */
#define HIGH_TEMPERATURE_WAIT_DURATION 300 // 高温時の待ち時間 10分 (300秒)
#endif /* DEBUG */
#define COMMAND_LINE_WAIT_DURATION 300     // 管理モード自動ログオフの待ち時間 5分 (300秒)

// 管理者コマンドラインで，温・湿度を測定する際のリトライ回数最大値
//#define SENSOR_MAX_RETRY 5

// Raspberry Piをshutdownする際の待ち時間
#define SHUTDOWN_PIN_DURATION    3       // shutdown信号のピンをHIGHに固定する時間(秒)
#define SHUTDOWN_WAIT_DURATION  10       // shutdown処理が走るのを待つ時間(秒)

// Raspberry Piをrebootする時の電源ON/OFFの操作の間の待ち時間
#define POWER_ON_WAIT 5                  // 確実に電源を切断するためOFF->5秒待ち->ONとする

// コンソール画面で一定時間間隔で運転状況を表示するための時間間隔
#define INFO_INTERVAL 10                // 10s

// パワースイッチを押して有効にするためのしきい値(時間)
#define POWER_PIN_THRESHOLD    2        // イベントチェック3回

