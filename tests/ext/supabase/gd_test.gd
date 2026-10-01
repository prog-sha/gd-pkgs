# Supabase GDExtensionをgd serveで動かし、自動待ち、GDTask、Err、sessionを確かめる試験。
extends RefCounted


# Supabase REST/Authの必要部分だけを返す試験server。
func respond(req: GDWebRequest) -> GDWebResponse:
	var target: String = req.target
	var body: Variant = null
	if req.method != "GET":
		var raw, _raw_err := req.text()
		body = null if raw.is_empty() else JSON.parse_string(raw)
	if target.begins_with("/auth/v1/token?grant_type=password"):
		return GD.web.json({"access_token": "access-test", "refresh_token": "refresh-test", "user": {"id": "user-1"}})!
	if target == "/auth/v1/settings":
		return GD.web.json({"disable_signup": false, "mailer_autoconfirm": true})!
	if target.begins_with("/auth/v1/token?grant_type=refresh_token"):
		return GD.web.json({"access_token": "access-refreshed", "refresh_token": "refresh-test", "user": {"id": "user-1"}})!
	if target == "/auth/v1/logout":
		return GD.web.text("", 204)
	if target.begins_with("/rest/v1/rpc/add"):
		return GD.web.json({"sum": body.a + body.b})!
	if target.begins_with("/rest/v1/missing"):
		return GD.web.json({"message": "not found"}, 404)!
	if target.begins_with("/rest/v1/slow"):
		await GD.async.sleep(5.0)
	if req.method == "GET":
		return GD.web.json([{"id": 1, "apikey": req.header("apikey"), "auth": req.header("authorization"), "path": target}])!
	return GD.web.json([{"method": req.method, "body": body, "path": target}])!


# 試験serverを立て、Database、Auth、取消を順に確かめる。
func main() -> int:
	var ck := GD.test.check()
	var app := GD.web.app()
	app.fallback(respond)
	var _listening, listen_err := app.listen(0, "127.0.0.1")
	ck.succeeds(listen_err, "fake server")
	var sb := GDSupabase.client("http://127.0.0.1:%d" % app.port(), "sb_publishable_test")

	# 通常名は完了まで待ち、行とErrを二値で返す。
	var rows, err := sb.select("items", {"select": "id,title", "limit": 2})
	ck.succeeds(err, "select")
	ck.eq(rows[0].apikey, "sb_publishable_test", "apikey header")
	ck.eq(rows[0].auth, "", "publishable key is not bearer")
	ck.ok("select=id%2Ctitle" in rows[0].path, "query encoded")
	var settings, settings_err := sb.auth_settings()
	ck.ok(settings_err == null and settings.has("disable_signup"), "auth settings")
	var inserted, insert_err := sb.insert("items", {"title": "hello"})
	ck.ok(insert_err == null and inserted[0].method == "POST", "insert")
	var updated, update_err := sb.update("items", {"title": "new"}, {"id": "eq.1"})
	ck.ok(update_err == null and updated[0].method == "PATCH", "update")
	var removed, remove_err := sb.remove("items", {"id": "eq.1"})
	ck.ok(remove_err == null and removed[0].method == "DELETE", "remove")
	var sum, rpc_err := sb.rpc("add", {"a": 2, "b": 3})
	ck.ok(rpc_err == null and sum.sum == 5, "rpc")

	# HTTPの失敗はErrの分類になり、応答本文を部分値として保つ。
	var missing, missing_err := sb.select("missing")
	ck.eq(missing, [], "failed select keeps its type")
	ck.fails(missing_err, Err.NOT_FOUND, "REST error kind")
	ck.eq(missing_err.partial, {"message": "not found"}, "REST error body")

	# _async名はGDTaskを返し、ほかの待ちの後でも結果を受け取れる。
	var started := sb.select_async("items")
	await GD.async.sleep(0.05)
	var later, later_err := await started
	ck.ok(later_err == null and later[0].id == 1, "task keeps its result")
	var both: Array = await GD.async.all([sb.select_async("items"), sb.rpc_async("add", {"a": 1, "b": 1})])
	ck.ok(both[0][1] == null and both[1][0].sum == 2, "tasks combine")
	var slow := sb.select_async("slow")
	slow.cancel()
	var _slow_rows, slow_err := await slow
	ck.fails(slow_err, Err.INTERRUPTED, "cancel a request")

	# Auth応答はclientのsessionへ反映される。
	var _signed, sign_err := sb.sign_in("test@example.com", "password")
	ck.ok(sign_err == null and sb.session.access_token == "access-test", "sign in")
	var authed, authed_err := sb.select("items")
	ck.ok(authed_err == null and authed[0].auth == "Bearer access-test", "session bearer")
	var _refreshed, refresh_err := sb.refresh()
	ck.ok(refresh_err == null and sb.session.access_token == "access-refreshed", "refresh")
	var _out, out_err := sb.sign_out()
	ck.ok(out_err == null and sb.session.is_empty(), "sign out")

	app.stop()
	print("failures=%d" % ck.failures)
	# serveは常駐するので、試験の終わりで明示的に終了する。
	Engine.get_main_loop().call("quit", ck.code())
	return ck.code()
