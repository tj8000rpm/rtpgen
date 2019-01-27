# rtpgen

DPDKを利用したRTPトラフィックジェネレータ

dpdk 17.11 l2fwd sampleベース

## 背景
- RTPの負荷印加装置を探しているものの、オープンソースのものを見つけられず
- 複数社のベンダに色々聞いてみるもみんなプロプラなものばっかりで話にならない
- それなりの負荷をそれなりの精度でかけようと思うとlinux socket programingでは辛い
- そういった背景からオープンソースで使えるRTPトラフィックジェネレータを作成

## 特徴
### DPDKを用いた高速パケット転送
- DPDKを用いた高速パケット送出（送信専用/受信パケットは破棄）
- 10G NIC(Intel X550)にて1cpuあたりptime=20msec, 50,000セッション(片方向のみ)を実現
- 50,000session * 50pps/session = 250,0000pps = 250K pps
- 250Kpps * 216 byte * 8bit = 4,320,000,000bps = 4.32Gbps
- もう少しPPSは稼げる余地がありますがとりあえずこのぐらいにしてます(define RTPGEN_RTP_MAX_SESSIONS)

### API
- protobuf_v2を用いたAPIを用意
- 別作成のREST-GWを用いることでweb api経由でパケットジェネレータの操作が可能
- https://github.com/tj8000rpm/rtpgen-restgw
- REST-GW等を使うことでsipp等との連携も可能

## 依存関係
- DPDK 17.11(もしかしたらもう少しあとのバージョンでも動くかも)
- protobuf-c
- CentOS7/ Makefileがcentos7でしかうまいこと動かない・・・  
  Makefileを少し工夫すればubuntuでもコンパイルはできます・・・そのうちこれは直します。

## コマンドラインオプション
- EAL option : EALオプションは適当にほかのドキュメントをご参照ください
- -p PORTMASK: l2fwdと同じportmask。0x01でポート1のみ利用。現状1ポートだけしか動かないはず
- -q NQ: l2fwdと同じnumber of queue。ポートあたりのQueueの数。多分いまは１じゃないと動かない・・
- -b BUFFER: mbufのサイズ。デフォルト8192MB。最大57344MB。  
             セッション数増やす場合はこちらも適宜増やす必要あり。(空きメモリがあればMAXに設定してけばとりあえずOK)
- -T PERIOD: l2fwdと同じ。統計情報表示のリフレッシュ間隔(sec)。(0 to disable, 10 default, 86400 maximum)
- -s SESSIONS: rtpgen独自。RTPの最大セッション数(上限 50,000 sessions)
- -M PORT0_MACPEER,PORT1_MACPEER,...,PORTn_MACPEER: ポート毎の対向MACアドレス。カンマ区切りでひつよポート数分設定
- -f FILE: RTPパケットのペイロードに入れる音源ファイル。音源はheaderlessのraw形式。末尾まで行くと循環する。
- -t PAYLOAD_TYPE: RTPペイロードタイプ、デフォルト0(PCM-U/G.711)
- -P SIZE: RTPパケットのペイロードサイズ（byte/パケット毎）。デフォルトでptime=20のPCMUの160byte
- -I PACKETIZATIOn_INTERVAL_MS: RTPのパケット化周期（送信間隔 msec）。デフォルトでptime=20msec

