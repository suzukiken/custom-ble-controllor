# map-controllor PCB (KiCad)

Seeed XIAO RP2040 (U1) と Alps RKJXT1F42001 (SW1) を載せる基板データです。

基板設計の仕様（製造制約・配線ルール・配線案）は [`AGENTS.md`](AGENTS.md) を参照してください。
AI エージェントに設計を任せる場合は、まず `AGENTS.md` を読ませてください。

## 構成



## 結線

| SW1 (RKJXT1F42001) | ネット | U1 (XIAO RP2040) |
| --- | --- | --- |


スイッチ・エンコーダの共通端子はすべて GND に落とし、
RP2040 の内蔵プルアップ(QMK の direct pins / encoder デフォルト)で読む。
外付け部品は不要。

## ファイル

- `encoder.kicad_pro` / `encoder.kicad_pcb` — KiCad 10 プロジェクト
- `footpritn/footpritn.pretty/` — XIAO RP2040 と RKJXT1F42001 のフットプリント
- `scripts/` — 配線・検証に使った pcbnew スクリプト
- `render_top.png` / `render_bottom.png` — レンダリング画像

## 検証

```bash
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc encoder.kicad_pcb
```

エラー0件・未結線0件(シルクとマスクの重なり警告のみ)。
