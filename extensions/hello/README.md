# Hello

C++やGDExtensionを使わない、1本の`.gd`だけで配るpackageの最小実例。

公式登録所`https://gd.progsha.com/pkg`から入れる。

```sh
gd install hello gd:@gd/hello@0.8.0
```

```gdscript
@import hello

func main():
	print(hello.message("gd"))
	return 0
```

`message`は文字列を返す。言語の`Err`二値ではない。

公開側は`gd.json`の`main`と`include`の両方へ`src/mod.gd`を書く。登録所では`mod.gd`という名前で配られる。本家Godotは`@import`を知らないので、`"place": "project"`で`pkg/<呼び名>/`へ置き、`const Hello := preload("res://pkg/hello/mod.gd")`で読む。利用側が呼び名を決めて`@import`するため、`class_name`は付けない。
