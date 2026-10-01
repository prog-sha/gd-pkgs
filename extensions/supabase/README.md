# Supabase GDExtension

SupabaseのDatabaseとAuthをgdから使う。公開名は`GDSupabase`と`GDSupabaseClient`。clientの型名は通常書かない。

公式登録所`https://gd.progsha.com/pkg`から入れる。

```sh
gd install ext:@gd/supabase@0.8.0
```

```gdscript
var sb := GDSupabase.client(
	GD.cli.env("SUPABASE_URL"),
	GD.cli.env("SUPABASE_PUBLISHABLE_KEY"),
)

func recent_posts() -> Array:
	var rows, err := sb.select("posts", {
		"select": "id,title,made_at",
		"order": "id.desc",
		"limit": 20,
	})
	if err:
		printerr(err.text())
		return []
	return rows
```

`select`、`insert`、`update`、`remove`、`rpc`、`auth_settings`、`sign_in`、`refresh`、`sign_out`は、gdの標準モジュールと同じく完了まで待ち、値と`Err`の二値を返す。`select`、`insert`、`update`、`remove`の値は行の配列、`auth_settings`、`sign_in`、`refresh`は辞書、`rpc`は関数の戻り値。失敗したときの値は同じ型の空の値になる。

末尾に`_async`を付けた名前は待たずに`GDTask`を返す。複数の要求を同時に始めるときや、途中で`cancel()`するときに使う。

```gdscript
var posts := sb.select_async("posts")
var settings := sb.auth_settings_async()
var rows, rows_err := await posts
var info, info_err := await settings
```

HTTPの失敗は`Err`の分類で返し、応答本文を`err.partial`に残す。

| HTTP | `Err` |
|---|---|
| 401 | `Err.UNAUTHENTICATED` |
| 403 | `Err.PERMISSION_DENIED` |
| 404 | `Err.NOT_FOUND` |
| 409 | `Err.ALREADY_EXISTS` |
| 429 | `Err.LIMITED` |
| その他の4xx | `Err.INVALID_DATA` |
| 5xx | 分類なし |

通信の失敗、期限切れ、取消は`GD.http`の`Err`をそのまま返す。通信は`GD.http`を使うので、`gd serve`でも動き、`--allow-net`の許可に従う。gd専用で、本家Godotでは使えない。

`client`はclientを返す。`session`はsessionの辞書、`url`は基準URL。Realtime、Storage、OAuth redirect、session永続化は含まない。

## build

Godot 4.7対応のgodot-cppを用意し、リポジトリのルートからbuildする。

```sh
git clone https://github.com/godotengine/godot-cpp tmp/ref_godot_cpp
git -C tmp/ref_godot_cpp checkout 9c8aeff0f58ad030f3d1030e8262de1322cd0ccd
uvx --from scons==4.10.1 scons godot_cpp=tmp/ref_godot_cpp out=tmp/build \
  build_profile=tools/build_profile.json platform=macos target=template_release arch=universal
```

projectへ`supabase.gdextension`と`bin/`を置き、extension一覧から起動時に読む。

`sb_secret_` keyと旧`service_role` JWTは拒否する。配布物にはpublishable keyだけを渡し、権限はSupabaseのRLSで制御する。
