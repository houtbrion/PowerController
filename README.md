# PowerController
常時運転するRaspberry Piの動作と周囲の環境をArduinoで監視し，
ArduinoでRaspberry Piに供給する電源を制御するシステム．

Raspberry Piが不調に陥った場合に，遠隔からRaspberry Piの電源を強制的にON/OFF
するためにArduinoを遠隔から制御するためのI/Fも提供する．

# 1. システム構成
システムは電源を制御される側のRaspberry Piと，Raspberry Pi用の電源を
制御するArduino，電源制御用端末であるArduinoに遠隔から手動でコマンドを
投入するための遠隔ログイン端末の3種類で構成される．

<img src="fig/システム構成全体.png" alt="システム構成図" width="800">

## 1.1 Raspberry PiとArduinoの連携
電源を制御される側のRaspberry PiのGPIOと電源を制御する側のArduinoのデジタル端子を
接続し，信号をやり取りすることで，人によるRaspberry Piの運用環境の監視やRaspbery Piの
動作の監視を自動化する．

* 強制shutdown信号
Raspberry PiのGPIO端子でArduinoの特定の端子の電圧をdaemonで監視し，Arduinoのある端子の
電圧がHIGHになった場合に，なにかの異常と判定し，shutdownを行う．

* keep alive信号
Raspberry PiのGPIOの特定のピンを定期的にHIにすることで，ArduinoにRaspberry Piが
生きていることを通知する．Arduino(Yun以外の機種の場合)では，Raspberry Piの
keep alive信号端子に接続されているデジタル端子を監視し，一定時間以内に
LOWからHIGHに変化した場合は，Raspberry Piが生きていると判断する．

また，一定時間以上Raspberry Piがkeep alive信号を出さない場合，
ArduinoはRaspberry Piが異常を起こしたと判定し，強制shutdown
信号を出し，一定時間待った後で，Raspbery Piの電源をOFFし，
数秒後に電源をONにする．

この機能を実現するために，割り込み端子が必要であるが，Yunでは端子不足
のため，何度か該当端子を監視し，端子がHIGHである回数で判定する．その
ため，Raspberry Piがkeep alive信号端子をHIGHにした状態でシステムが
フリーズしているとYunは誤判定してしまう．

* CPU高温信号
Raspberry PiのカーネルのCPU温度測定機能を用いて，CPUの温度を常駐
するdaemonで取得し，ある温度以上であった場合は，緊急shutdownが
必要と見なして，Arduinoに通知した後，自らshutdownを行う．

Arduinoは，この信号を受信すると，自らもRaspbery Piに
強制shutdown信号を出して，一定時間後に電源をOFFにした
上で，一定の時間(Arduinoのプログラムで定義しているが数分～数十分間)
電源OFF状態を維持して冷却を待つ．
その後，Raspberry Piの電源をONにする．

## 1.2 Arduinoによる環境監視
電源制御端末であるArduinoでは，上に述べたRaspberry Piの状態監視の他に，これらのシステムが置いてある環境(温度・湿度)の
監視を行っている．

環境の温度が高い状態でシステムの動作を続けていると，高温となるRaspberry PiのCPUや無線回路が破損する可能性が
あるため，環境の温度が高い場合は，Raspberry Piの強制shutdownと電源OFFを行う．その上で，環境の温度が下がる
のを待ち，温度低下後にRaspberry Piの電源をONにする．

## 1.3 遠隔ログイン関連
Raspberry Piに異常が発生し，ハードリセットが必要な場合や，Raspberry PiのCPUが発熱した場合，設置場所が非常に高温になった場合など，
Raspberry Piへの電源供給を強制的に止めるなどの必要があり，そのため，外部からネットワーク経由で電源制御端末である
Arduinoにコマンドを入力する
ための端末．

sshは暗号化処理にメモリとCPUパワーが必要なため，Arduino等のマイコンでは困難．もし，ssh接続が必須となる場合は，
ssh用にRaspberry Piを利用するか，Linuxで動作するモジュールが搭載された機種(Arduino YunやIntel Edison for Arduino)を
利用する必要がある．sshの代わりに，telnetを用いる場合，MCU内蔵のWiFiモジュールが利用可能となるため，
今回の試作では，ESP-WROOM2を搭載した端末(スイッチサイエンスのESPr One)を利用した．
<img src="fig/遠隔ログイン端末.png" alt="遠隔ログイン" width="800">

### Yunを用いる場合
Arduino Yunを用いる場合は，電源制御端末と遠隔ログイン用端末が1つの筐体に収まるため，
構成が非常に単純になる．Intel Edison for Arduinoの場合も似た構成になるはずであるが，
試していない．
<img src="fig/Yunで全部.png" alt="Yunで兼任" width="800">

手元のハードで簡単なテストをしたところ，以下の2つの問題に遭遇した．
解決するためには，いろいろハード，ソフト両面で工夫が必要な模様．

- UnoやMegaの場合と，Yunでは動作が異なり，Raspberry Piとの接続線に
ロジックレベル変換回路を用いた場合，Yunの端子(データ読み取り専用)が常に電圧HIGHとなる
ため，動作がうまくいかない．また，Raspberry PiからArduinoに信号を送るために
2つの端子を用い，UnoやMegaでは割り込み処理で対応している．
しかし，Yunでは利用可能な割り込み信号端子5個のうち，4つが
コンソールのシリアルとI2Cの端子と被っているため，
割り込み以外で実装する必要がある．

- MegaやUnoで利用しているRaspberry Pi用電源回路をArduinoのシールド基板に載せたもの
とRaspberry Piと相互接続するためのインターフェース回路(シールド)の2つを同時に
Yun本体に載せると，Yunがまともにブートしない．どちらか，片方のシールドのみだと
動作する． これは，USB経由で給電している電力では，すべてのシールドを駆動する
ために必要な電力を供給できないことが理由．VIN端子にUSBより大きな電力を
供給することが必要．

### ESP-WROOM2を用いる場合
現在のところ，スイッチサイエンスのESPr Oneでテストしているが，
以下の様な問題が発生している．

- ブート時にUSB接続のコンソールをTeraTerm等のコンソールアプリでオープンしないと，ファームが動作しない．
  これは，USB端子を利用せず，ACアダプタで電源供給することで回避できる．
- Arduino側がソフトウェアシリアルではうまく通信できないため，通常のシリアルポートを利用する必要がある．
  標準のシリアルを用いるか，Unoではなく，Megaのようにシリアルが複数ある端末を利用する．
- ArduinoとESPr Oneの間のシリアル接続をオープンすると，接続直後はゴミデータ(ブートメッセージ)が大量にESPr Oneから
  発生し，解釈できないコマンドが大量にArduinoに入力される．

2番目の問題は，ESP8266側もソフトウェアシリアルに変更し，ソフトウェアシリアル同士の
接続にしても改善せず，ESP8266側をソフトウェアシリアル，Arduino側をハードシリアル
でも接続できない．両方ハードシリアルであることが必要．

3番目の問題は，ESP8266のuart0のTxとRxの代わりに，I2CとかSPIのシリアル変換チップを利用することが考えられるが，
スイッチサイエンスのSC16IS750を搭載したモジュールで試した所，ハードウェアフロー制御の端子を用いず，
RxとTxだけで接続(USBシリアル変換経由でPCに接続)すると，文字化けが激しく利用できない状態であった．
Rx,Tx,GNDだけでArduinoと接続して動作する別種のI/Fが必要．

## 1.4 Raspberry Pi自身のCPU温度制御
Raspberry Piで動作するdaemonでは，CPUの温度を監視して，CPU温度が高すぎる場合に
Arduinoと連携して，shutdownや電源ON/OFF
ファンのON/OFFを行うが，単独でもファンのON/OFFを行い，CPU温度の調整をする．

# 2. ソフト
## 2.1 Raspberry Pi用モジュール「powerMNG」
電源を供給される側のRaspberry Piで動作するdaemonで以下の機能を実装．
- 電源制御端末からのShutdown指示信号を監視し，shutdownを実行
- 自分自身のCPU温度を監視し，ファンを制御する信号をON/OFFした上，CPU温度が高すぎる場合に，電源制御端末に異常を通知した後，shutdownを実行

## 2.2 PowerController
周辺の温度と湿度，電源ボタンを監視し，Raspberry Piに供給する電源をON/OFFするArduino用プログラム．
また，周辺の温度・湿度や電源制御状況をLCDに表示する機能もある．

以下は主な機能．
- 周辺の温度が高くなると，Raspberry PiにShutdown信号を出し，一定時間経過後に電源をOFFし，周辺温度が下がるまでその状態を維持
- Raspberry PiからCPU高温信号が来ると，一定時間経過後に電源をOFFし，一定時間経過後に周辺温度が規定値以下なら電源をON
- Raspberry Pi動作中に電源ボタンが押されると，Raspberry PiにShutdown信号を出し，一定時間経過後に電源をOFFし，その状態(強制電源OFF状態)を維持
- 強制電源OFF状態で電源ボタンが押されると，周辺温度を検査し，問題なければRaspberry Piへの電源供給を開始

## 2.3 WiFiTelnetToSerial
遠隔ログイン用にESP-WROOOM2を用いる場合のWROOM2用プログラムで，telnetで接続した場合に，認証を行い，
コマンド入力待ちとなる．コマンド入力待ち状態で接続コマンドが入力されると，電源制御端末のシリアルと中継を行う．
ログアウトする場合は，Linuxのminicomと同じくCtrl-Aでエスケープした後，ログアウトコマンドを入力する．

# 3. ハード

以下の回路(各種マイコン用シールド)は基本的に，電源制御端末は5V動作のもの，それ以外は3.3V駆動を
想定しているが，一部のハードモジュールは5V,3.3Vのどちらでも大丈夫なように設計している．
電源制御回路以外はどちらでも大丈夫なはずであるが，テストしていないため，注意していただきたい．

電源制御端末にArduino Zero等の3.3V動作の端末を用いる場合は，電源制御回路の信号用リレーを3.3V動作の
ものと交換することで対応できるはずである．

以下，幾つかの相互接続用のPiとArduino用のシールド基板の図面を示すが，図面そのものは『ハード関係』ディレクトリに収容している．また，Mega用のシールド基板の図面などもあるため，『ハード関係』ディレクトリを良く見ていただきたい．

## 3.1 Raspberry Pi関係

## 3.1 電源制御シールドUNO用
### ユニバーサル基板図面
<img src="fig/Arduino電源制御回路-UNOシールド_ブレッドボード.jpg" alt="Arduino電源制御回路-UNOシールド_ブレッドボード" width="400">

### 回路図
<img src="fig/Arduino電源制御回路-UNOシールド_回路図.jpg" alt="Arduino電源制御回路-UNOシールド_回路図" width="400">


## 3.2 電源制御シールドYun用
### ユニバーサル基板図面
<img src="fig/Arduino電源制御回路-yunシールド_ブレッドボード.jpg" alt="Arduino電源制御回路-yunシールド_ブレッドボード" width="400">
### 回路図
<img src="fig/Arduino電源制御回路-yunシールド_回路図.jpg" alt="Arduino電源制御回路-yunシールド_回路図" width="400">

## 3.3 Pi-Arduino相互接続シールド Arduino用
### ユニバーサル基板図面
<img src="fig/Pi-Arduino相互接続シールド-Uno用_ブレッドボード.jpg " alt="Pi-Arduino相互接続シールド-Uno用_ブレッドボード" width="400">
### 回路図
<img src="fig/Pi-Arduino相互接続シールド-Uno用_回路図.jpg" alt="Pi-Arduino相互接続シールド-Uno用_回路図" width="400">


## 3.4 Pi-Arduino相互接続シールド Pi用
### ユニバーサル基板図面
<img src="fig/Pi-Arduino相互接続シールド-Pi用_ブレッドボード.jpg" alt="Pi-Arduino相互接続シールド-Pi用_ブレッドボード" width="400">
### 回路図
<img src="fig/Pi-Arduino相互接続シールド-Pi用_回路図.jpg" alt="Pi-Arduino相互接続シールド-Pi用_回路図" width="400">

## 3.5 WROOM2-Arduino相互接続シールド Arduino用
### ユニバーサル基板図面
<img src="fig/WROOM2-Arduino相互接続シールド-Arduino用_ブレッドボード.jpg" alt="WROOM2-Arduino相互接続シールド-Arduino用_ブレッドボード" width="400">
### 回路図
<img src="fig/WROOM2-Arduino相互接続シールド-Arduino用_回路図.jpg" alt="WROOM2-Arduino相互接続シールド-Arduino用_回路図" width="400">

## 3.6 WROOM2-Arduino相互接続シールド WROOM2用
### ユニバーサル基板図面
<img src="fig/WROOM2-Arduino相互接続シールド-WROOM2用_ブレッドボード.jpg" alt="WROOM2-Arduino相互接続シールド-WROOM2用_ブレッドボード" width="400">
### 回路図
<img src="fig/WROOM2-Arduino相互接続シールド-WROOM2用_回路図.jpg" alt="WROOM2-Arduino相互接続シールド-WROOM2用_回路図" width="400">
