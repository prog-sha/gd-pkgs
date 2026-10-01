#!/bin/bash
# Supabase GDExtensionをbuildし、gd serveとlocalhostの試験serverで検証する。
set -eu
cd "$(dirname "$0")/.."

GD=${GD:-./bin/gd.macos.template_release.arm64}
PROJECT=tmp/supabase_ext_test
LIB=tmp/supabase_ext_bin/libgdsupabase.macos.template_release.universal.dylib

# 生成物をtmpへ揃えて本体sourceと混ぜない。
uvx --from scons==4.10.1 scons godot_cpp=tmp/ref_godot_cpp out=tmp build_profile=tools/build_profile.json \
	platform=macos target=template_release arch=universal -j12 >/dev/null
rm -rf "$PROJECT"
mkdir -p "$PROJECT/bin" "$PROJECT/.godot"
cp tests/ext/supabase/gd_test.gd tests/ext/supabase/typed_test.gd tests/ext/supabase/packed_test.gd \
	tests/ext/supabase/live_test.gd extensions/supabase/supabase.gdextension "$PROJECT/"
cp tests/ext/supabase/extension_list.cfg "$PROJECT/.godot/"
cp "$LIB" "$PROJECT/bin/"
GD_PATH=$(cd "$(dirname "$GD")" && pwd)/$(basename "$GD")

# 起動時に読んだ拡張で、自動待ち、GDTask、Err、sessionをgd serveで通す。
(cd "$PROJECT" && "$GD_PATH" --allow-ext --allow-net serve gd_test.gd)

# gd固有の内部型、二値、GDTaskをstrictで推論する。
(cd "$PROJECT" && "$GD_PATH" --allow-ext --strict check typed_test.gd)

# compile成果物にもmanifestとnative libraryを同梱し、単体で起動する。
(cd "$PROJECT" && "$GD_PATH" compile --output packed packed_test.gd >/dev/null)
(cd "$PROJECT" && ./packed)
