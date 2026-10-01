# Supabase内部clientと結果の型を、型名なしでstrict推論できるか調べる。
extends RefCounted

var sb := GDSupabase.client("https://example.supabase.co", "sb_publishable_test") # 内部型を推論するclient


# 通常名の二値、_async名のGDTask、await後の型をstrictで検査する。
func inspect_result() -> bool:
	var rows, err := sb.select("items")
	var count: int = rows.size()
	var task: GDTask = sb.auth_settings_async()
	var settings, settings_err := await task
	var keys: Array = settings.keys()
	return err == null and settings_err == null and count + keys.size() >= 0


# strict検査だけに使うので実行はしない。
func main() -> int:
	return 0
