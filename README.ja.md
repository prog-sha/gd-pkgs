# gd packages

[English](README.md)

**GD 0.8** と **Godot 4.7.2** 向け package **0.8.0**。この版は [prog-sha/gd-pkgs](https://github.com/prog-sha/gd-pkgs) で公開している。

公式登録所 `https://gd.progsha.com/pkg` から入れる。

```sh
gd install hello gd:@gd/hello@0.8.0
gd install discord gd:@gd/discord@0.8.0
gd install ext:@gd/supabase@0.8.0
```

純GDScriptは`@import`で読む。native拡張はmanifestのclass名を使う。本家Godotから読むときは`gd.json`へ`"place": "project"`を書き、`const Hello := preload("res://pkg/hello/mod.gd")`のように`pkg/<呼び名>/`を読む。依存は`gd.lock`へ版ごと固定する。

| package | 入口 | 用途 |
|---|---|---|
| `@gd/hello` | `@import hello` | 純GDScript packageの最小例 |
| `@gd/discord` | `@import discord` | Discordの文字Bot |
| `@gd/supabase` | `GDSupabase` | Supabase DatabaseとAuth |

REST系の完了値は辞書`{ok, status, data, error, kind, headers}`。`kind`は現行`Err`の分類名と同じ綴りの文字列で、言語の`Err`二値ではない。詳細は各README。

- [Hello](extensions/hello/README.md)
- [Discord](extensions/discord/README.md)
- [Supabase](extensions/supabase/README.md)

## 対応環境

- Godot 4.7.2
- GD 0.8.0
- macOS arm64
- Linux x86_64
- Windows x86_64

native拡張はprocessと同じ権限で動く。信頼するpackageとversionだけを`gd.lock`で固定し、commitする。

パッケージの作成、ビルド、公開手順は[パッケージを作る](AUTHORING.md)。公式登録所への公開にはscopeごとの権限が必要。
