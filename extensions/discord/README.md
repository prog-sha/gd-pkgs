# Discord Bot

Discord Gateway v10とRESTを使う純GDScriptの文字Bot package。C++、GDExtension、platform別binaryは使わない。

公式登録所`https://gd.progsha.com/pkg`から入れる。

```sh
gd install discord gd:@gd/discord@0.8.0
```

```gdscript
@import discord

var bot


# Botを起動し、Gatewayの準備完了を待つ。
func main():
	var token := OS.get_environment("DISCORD_TOKEN")
	bot = discord.bot(token, {
		"intents": discord.GUILDS | discord.GUILD_MESSAGES | discord.MESSAGE_CONTENT,
	})
	if bot == null:
		return 1
	bot.event.connect(on_event)
	if bot.start() != OK:
		return 1
	await bot.ready
	while bot.started:
		await Engine.get_main_loop().process_frame
	return 0


# `!ping`へ返信する。
func on_event(name, data):
	if name == "MESSAGE_CREATE" and data.content == "!ping":
		var reply = await bot.send_message(data.channel_id, "pong")
		if not reply.ok:
			printerr(reply.error)
```

`bot`はclientか`null`を返す。`start`と`set_presence`はGodotの`Error`（`OK`など）を返す。`close`は値を返さない。

`request`、`send_message`、`edit_message`、`delete_message`はSignalを返す。`await`の結果は辞書`{ok, status, data, error, kind, headers}`で、言語の`Err`二値ではない。`kind`は現行`Err.name_of`と同じ綴りの文字列。空文字は成功、または次に当てはまらない失敗。

| `kind` | とき |
|---|---|
| `Unauthenticated` | HTTP 401 |
| `PermissionDenied` | HTTP 403 |
| `NotFound` | HTTP 404 |
| `InvalidData` | その他のHTTP 4xx、または不正な要求 |
| `Limited` | HTTP 429、またはREST queueの上限 |
| `TimedOut` | 応答期限 |
| `Interrupted` | clientを閉じた後 |
| `Unsupported` | 未対応のHTTP method、または要求を開始できない |

Gatewayは`ready`、`event`、`resumed`、`disconnected`、`failed` signalを出す。heartbeat、Resume、429待機、globalとrouteのrate limitを処理する。

`set_presence({"since": null, "activities": [], "status": "online", "afk": false})`でpresenceを更新できる。連続更新は最新値へまとめ、過去20秒で5回まで。RESTは直列実行し、待ちbody総量は既定16 MiBまで。Stringのmessageはmentionを発火させない。mentionが必要ならDictionaryの`allowed_mentions`で明示する。

一つのprocess内ではtokenごとのglobal・bucket・Identify制限を全clientで共有する。純GDScriptにはprocess間を排他的に更新するportableなfile lockがない。複数processや複数machineで同じtokenを使うときは、一つのBot processへ集約するかREST proxyを使う。

tokenは`.env`の`DISCORD_TOKEN`から渡し、source、`gd.json`、配布物へ入れない。strictではDiscordと環境変数だけを許可する。

```sh
gd --strict --allow-env=DISCORD_TOKEN \
	--allow-net=discord.com:443,*.discord.gg:443 run bot.gd
```

GatewayにはGodotの`WebSocketPeer`、RESTには`HTTPRequest`を使う。通信にはSceneTreeが必要なので`gd run`で起動し、Botを閉じるまでframeを待つ。Discord Voiceは対象外。

## package開発

公開入口は`src/mod.gd`。`include`もそのfileだけを明示する。

```sh
gd check extensions/discord/src/mod.gd
gd fmt --check extensions/discord/src/mod.gd
GD=/path/to/gd bash tests/discord.sh
```

`GODOT`を本家Godot 4.7.2の実行fileにすると、同じfake server試験をGodotでも走る。
