# 公開キーで実SupabaseのAuth設定へ接続する手動試験。
extends RefCounted


# 秘密値や応答内容を表示せず、接続結果だけを返す。
func main() -> int:
	var url: String = GD.cli.env("SUPABASE_URL", "")
	var key: String = GD.cli.env("SUPABASE_PUBLISHABLE_KEY", "")
	if url.is_empty() or key.is_empty():
		print("live_supabase=false")
		return 2
	var sb := GDSupabase.client(url, key)
	var _settings, err := sb.auth_settings()
	print("live_supabase=%s" % (err == null))
	return 0 if err == null else 1
