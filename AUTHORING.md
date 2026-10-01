# パッケージを作る

[English](README.md) · [日本語](README.ja.md)

GD 0.8 向けの手順です。package 0.8.0 は [prog-sha/gd-pkgs](https://github.com/prog-sha/gd-pkgs) にあります。

## GDScript

`src/mod.gd`を公開入口にし、`gd.json`で配るファイルを明示します。

```json
{
  "name": "@scope/greet",
  "version": "1.0.0",
  "main": "src/mod.gd",
  "include": ["src/mod.gd"],
  "description": "挨拶を返すパッケージ"
}
```

```gdscript
# Return a greeting for the supplied name.
extends RefCounted

static func message(name = "world"):
    return "hello, %s" % name
```

`include`は入口と同じディレクトリ以下のファイルを指定します。必要なファイルだけ列挙し、作業ディレクトリ全体を含めないでください。ライセンスなども配る場合は入口と同じディレクトリへ置き、`include`へ加えます。

パッケージ内部のファイルは相対パスで`preload`します。他のパッケージへの依存は`gd.json`の`imports`に書きます。本家Godotでも動くパッケージには`"godot": true`を指定し、パッケージ内部でGD専用の構文やAPIを使わないでください。

利用側では次のように読みます。

```sh
gd install greet gd:@scope/greet@1.0.0
```

```gdscript
@import greet

func main():
    print(greet.message("world"))
```

本家Godotでは`gd.json`に`"place": "project"`を書いて導入し、`preload("res://pkg/greet/mod.gd")`で読みます。

## ネイティブ拡張

SupabaseのC++ソースとmanifestは`extensions/supabase/`にあります。Godot 4.7.2向けのbindingでビルドします。

```sh
git clone https://github.com/godotengine/godot-cpp tmp/ref_godot_cpp
git -C tmp/ref_godot_cpp checkout 9c8aeff0f58ad030f3d1030e8262de1322cd0ccd
uvx --from scons==4.10.1 scons godot_cpp=tmp/ref_godot_cpp out=tmp/build \
  build_profile=tools/build_profile.json platform=macos target=template_release arch=universal
```

`platform`、`target`、`arch`をmanifestに宣言した組合せに合わせてビルドします。Windowsのクロスビルドは`use_mingw=yes`を指定します。GD専用APIのbindingを使う場合は、そのGD実行体から生成したAPIと対応するbindingを指定してください。

`[libraries]`は各OSとCPUのライブラリ、`[classes]`は公開クラス名、`[await]`は待った後の値の型です。`Pair:型`と書くと、値と`Err`の二値になります。

gdの標準モジュールと同じ呼び方にするには、同じ実装を通常名と`_async`名の2つで登録し、`[await]`に両方を書きます。通常名は完了まで自動で待ち、`_async`名は待たずに`GDTask`を返します。完了のSignalは値と`Err`の2つを渡します。

```ini
[await]
GDSupabaseClient.select = "Pair:Array"
GDSupabaseClient.select_async = "Pair:Array"
```

## 公開する

`gd.json`があるディレクトリで公開します。書込み権限のある登録所を`GD_REGISTRY`に設定し、scopeに対応したtokenを`GD_TOKEN`で渡します。tokenをファイルやコマンド履歴に残さないでください。

```sh
gd publish --dry-run
gd publish
```

公式登録所の`@gd`へは、そのscopeの公開権限が必要です。公開済みの同じ版は上書きできません。

利用側の`gd.json`に`registry`がある場合は、そのURLが`GD_REGISTRY`より優先します。依存を再現するときは`gd.json`と`gd.lock`を保存し、`gd install --frozen`を使います。取得済みのキャッシュだけで復元する場合は`--cached-only`を併用します。

`gd install`はパッケージ引数があると依存を追加し、引数がないと設定済みの依存を復元します。`--frozen`・`--cached-only`・`--sync`は引数なしの復元に使います。
