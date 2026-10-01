#!/bin/bash
# Discord純GDScript packageを利用者layoutへ置き、localhost通信を検証する。
set -eu
cd "$(dirname "$0")/.."

GD=${GD:-} # gd 0.8.0の実行file。指定したときfake server試験をgdで走る
GODOT=${GODOT:-} # 本家Godot 4.7.2の実行file。指定したとき同じ試験をGodotで走る
LIMIT=${LIMIT:-90} # 試験を待つ最大秒数
PROJECT=tmp/discord_gd_test # 利用者projectの生成先

if [ -z "$GD" ] && [ -z "$GODOT" ]; then
	echo "GD or GODOT is required" >&2
	exit 2
fi

# 子processを期限内だけ動かす。
run_limited() {
	perl -e '$limit = shift; alarm $limit; exec @ARGV or exit 127' "$LIMIT" "$@"
}

rm -rf "$PROJECT"
mkdir -p "$PROJECT/discord"
cp tests/ext/discord/project.godot "$PROJECT/project.godot"
cp tests/ext/discord/fake_server.gd tests/ext/discord/late_test.gd "$PROJECT/"
cp extensions/discord/src/mod.gd "$PROJECT/discord/mod.gd"

# 一つのengineでGatewayとRESTの検査結果を確かめる。
run_one() {
	local output
	local code
	set +e
	output=$(run_limited "$@" 2>&1)
	code=$?
	set -e
	printf '%s\n' "$output"
	test "$code" -eq 0
	grep -q '^checks=26 failures=0$' <<<"$output"
	! grep -Eq 'SCRIPT ERROR|^ERROR:' <<<"$output"
}

if [ -n "$GD" ]; then
	(cd "$PROJECT" && run_one "$GD" --no-header run late_test.gd)
fi
if [ -n "$GODOT" ]; then
	run_one "$GODOT" --headless --path "$PROJECT" --script res://late_test.gd
fi
