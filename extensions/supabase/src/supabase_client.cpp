/**************************************************************************/
/*  supabase_client.cpp                                                   */
/**************************************************************************/
/*                          gd packages                                  */
/**************************************************************************/

// Supabase GDExtensionのHTTP、Database、Auth実装。宣言はsupabase_client.h。

#include "supabase_client.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// JSON応答から外へ出せる失敗文を選ぶ。
static String error_text(const Variant &p_data, const String &p_fallback) {
	if (p_data.get_type() != Variant::DICTIONARY) {
		return p_fallback;
	}
	const Dictionary body = p_data;
	const PackedStringArray keys = { "error_description", "message", "msg", "error" };
	for (const String &key : keys) {
		const String text = body.get(key, "");
		if (!text.is_empty()) {
			return text;
		}
	}
	return p_fallback;
}

// 旧JWT keyのpayloadからservice_role権限を見分ける。
static bool is_legacy_secret(const String &p_key) {
	const PackedStringArray parts = p_key.split(".");
	if (parts.size() != 3) {
		return false;
	}
	String payload = parts[1].replace("-", "+").replace("_", "/");
	while (payload.length() % 4 != 0) {
		payload += "=";
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(Marshalls::get_singleton()->base64_to_utf8(payload)) != OK) {
		return false;
	}
	const Variant data = json->get_data();
	return data.get_type() == Variant::DICTIONARY && Dictionary(data).get("role", "") == "service_role";
}

// HTTP状態をgdのErr分類の生成関数名へ直す。分類の無い失敗は空文字。
static String err_kind(int64_t p_status) {
	switch (p_status) {
		case 401:
			return "unauthenticated";
		case 403:
			return "permission_denied";
		case 404:
			return "not_found";
		case 409:
			return "already_exists";
		case 429:
			return "limited";
		default:
			return p_status >= 400 && p_status < 500 ? "invalid_data" : String();
	}
}

// gdのErrを分類つきで作る。分類が無ければ理由だけのErrにする。
static Variant make_err(const String &p_kind, const String &p_message) {
	return p_kind.is_empty() ? ClassDBSingleton::get_singleton()->class_call_static("Err", "from", p_message)
							 : ClassDBSingleton::get_singleton()->class_call_static("Err", p_kind, p_message);
}

// 本文をJSONとして読み、読めなければ文字列のまま返す。
static Variant body_data(const String &p_text) {
	if (p_text.is_empty()) {
		return Variant();
	}
	Ref<JSON> json;
	json.instantiate();
	return json->parse(p_text) == OK ? json->get_data() : Variant(p_text);
}

// 失敗時にも宣言した型どおりの空の値を返す。
Variant GDSupabaseCallInternal::empty() const {
	switch (shape) {
		case SHAPE_ARRAY:
			return Array();
		case SHAPE_DICTIONARY:
			return Dictionary();
		default:
			return Variant();
	}
}

// GD.httpの応答を値とErrへ直す。通信の失敗はGD.httpのErrをそのまま渡す。
void GDSupabaseCallInternal::completed(const Variant &p_response, const Variant &p_error) {
	fetching = Signal();
	Object *response = p_response;
	const Object *failure = p_error;
	if (failure) {
		finish(empty(), p_error);
		return;
	}
	if (!response) {
		finish(empty(), make_err("interrupted", "Supabase response is missing"));
		return;
	}
	const int64_t status = response->get("status");
	const Variant data = body_data(response->call("text"));
	if (status < 200 || status >= 300) {
		const Variant error = make_err(err_kind(status), vformat("HTTP %d: %s", status, error_text(data, "request failed")));
		finish(empty(), static_cast<Object *>(error)->call("with_partial", data));
		return;
	}
	// Auth応答だけclientのsessionへ反映する。
	if (client.is_valid()) {
		if (action == ACTION_SESSION && data.get_type() == Variant::DICTIONARY) {
			client->accept_session(data);
		} else if (action == ACTION_SIGN_OUT) {
			client->clear_session();
		}
	}
	finish(data.get_type() == Variant::NIL ? empty() : data, Variant());
}

// 開始前の失敗を次のturnでSignalへ流す。
void GDSupabaseCallInternal::failed(const String &p_message) {
	finish(empty(), make_err("unsupported", p_message));
}

// 値とErrを通知し、自己参照を片付ける。
void GDSupabaseCallInternal::finish(const Variant &p_value, const Variant &p_error) {
	Ref<GDSupabaseCallInternal> keep = self_hold; // 通知中に最後の参照が消えても完了まで生かす。
	emit_signal("finished", p_value, p_error);
	client.unref();
	self_hold.unref();
}

// GD.httpで通信を始める。gd以外では開始前の失敗として知らせる。
void GDSupabaseCallInternal::start(const Ref<GDSupabaseCallInternal> &p_self, const Ref<GDSupabaseClient> &p_client,
		const String &p_url, HTTPClient::Method p_method, const Dictionary &p_headers,
		const String &p_body, Action p_action, Shape p_shape) {
	self_hold = p_self;
	client = p_client;
	action = p_action;
	shape = p_shape;
	Object *gd = Engine::get_singleton()->has_singleton("GD") ? Engine::get_singleton()->get_singleton("GD") : nullptr;
	Object *http = gd ? static_cast<Object *>(gd->get("http")) : nullptr;
	if (!http) {
		call_deferred("_failed", "Supabase needs the gd runtime");
		return;
	}
	static const char *const METHODS[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "TRACE", "CONNECT", "PATCH" }; // HTTPClient::Methodの順の名前
	Dictionary opts;
	opts["method"] = METHODS[p_method];
	opts["headers"] = p_headers;
	opts["timeout"] = 30.0;
	opts["max_body"] = 16 * 1024 * 1024;
	if (!p_body.is_empty()) {
		opts["body"] = p_body;
	}
	const Variant pending = http->call("fetch_async", p_url, opts);
	if (pending.get_type() != Variant::SIGNAL) {
		call_deferred("_failed", "Supabase request could not start");
		return;
	}
	fetching = pending;
	fetching.connect(Callable(this, "_completed"), CONNECT_ONE_SHOT);
}

// 通信を取り消す。結果は中断のErrとして届く。
void GDSupabaseCallInternal::cancel() {
	Object *operation = fetching.get_object();
	if (operation && operation->has_method("cancel")) {
		operation->call("cancel");
	}
}

// 通信CallのmethodとSignalを登録する。
void GDSupabaseCallInternal::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_completed", "response", "error"), &GDSupabaseCallInternal::completed);
	ClassDB::bind_method(D_METHOD("_failed", "message"), &GDSupabaseCallInternal::failed);
	ClassDB::bind_method(D_METHOD("cancel"), &GDSupabaseCallInternal::cancel);
	ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT), PropertyInfo(Variant::OBJECT, "error", PROPERTY_HINT_RESOURCE_TYPE, "Err")));
}

// table、function、schemaへ使えるASCII識別子か調べる。
bool GDSupabaseClient::safe_name(const String &p_name) {
	if (p_name.is_empty()) {
		return false;
	}
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
			return false;
		}
	}
	return true;
}

// query値をpercent encodeしてURLへ繋ぐ。
String GDSupabaseClient::query_text(const Dictionary &p_query) {
	PackedStringArray parts;
	for (const Variant &raw_key : p_query.keys()) {
		const String key = raw_key;
		const String value = p_query[raw_key];
		parts.push_back(key.uri_encode() + "=" + value.uri_encode());
	}
	return parts.is_empty() ? String() : "?" + String("&").join(parts);
}

// 共通認証とschemaの見出しを組む。
Dictionary GDSupabaseClient::headers(bool p_json, bool p_mutation, bool p_rest, const Dictionary &p_more) const {
	Dictionary out;
	out["apikey"] = api_key;
	out["Accept"] = "application/json";
	if (p_json) {
		out["Content-Type"] = "application/json";
	}
	const String token = auth_session.get("access_token", "");
	if (!token.is_empty()) {
		out["Authorization"] = "Bearer " + token;
	} else if (!api_key.begins_with("sb_publishable_") && !api_key.begins_with("sb_secret_")) {
		out["Authorization"] = "Bearer " + api_key;
	}
	if (p_rest) {
		out[p_mutation ? "Content-Profile" : "Accept-Profile"] = schema;
	}
	// 利用者の見出しは既定値を上書きできる。改行を含むものはGD.httpが拒否する。
	out.merge(extra_headers, true);
	out.merge(p_more, true);
	return out;
}

// REST tableのURLを安全な名前から組む。
String GDSupabaseClient::table_url(const String &p_table, const Dictionary &p_query) const {
	ERR_FAIL_COND_V_MSG(!safe_name(p_table), String(), "table must be an ASCII identifier");
	return base_url + String("/rest/v1/") + p_table + query_text(p_query);
}

// HTTP Callを作り、待てるSignalを返す。
Signal GDSupabaseClient::send(const String &p_url, HTTPClient::Method p_method, const String &p_body, const Dictionary &p_more,
		GDSupabaseCallInternal::Action p_action, GDSupabaseCallInternal::Shape p_shape, bool p_rest) {
	Ref<GDSupabaseCallInternal> call;
	call.instantiate();
	const Ref<GDSupabaseClient> owner = Variant(this);
	call->start(call, owner, p_url, p_method, headers(!p_body.is_empty(), p_method != HTTPClient::METHOD_GET, p_rest, p_more), p_body, p_action, p_shape);
	return Signal(call.ptr(), "finished");
}

// Auth成功時に新しいsessionを控える。
void GDSupabaseClient::accept_session(const Dictionary &p_session) {
	auth_session = p_session;
}

// sign out成功時にsessionを空にする。
void GDSupabaseClient::clear_session() {
	auth_session.clear();
}

// project URL、key、schema等を設定する。
void GDSupabaseClient::setup(const String &p_url, const String &p_key, const Dictionary &p_opts) {
	base_url = p_url.trim_suffix("/");
	api_key = p_key;
	schema = p_opts.get("schema", "public");
	extra_headers = p_opts.get("headers", Dictionary());
}

// tableから行を選ぶ。
Signal GDSupabaseClient::select(const String &p_table, const Dictionary &p_query) {
	Dictionary query = p_query.duplicate();
	if (!query.has("select")) {
		query["select"] = "*";
	}
	return send(table_url(p_table, query), HTTPClient::METHOD_GET, String(), Dictionary(), GDSupabaseCallInternal::ACTION_NONE, GDSupabaseCallInternal::SHAPE_ARRAY);
}

// tableへ行を追加する。
Signal GDSupabaseClient::insert(const String &p_table, const Variant &p_rows, bool p_upsert) {
	Dictionary more;
	more["Prefer"] = String("return=representation") + (p_upsert ? ",resolution=merge-duplicates" : "");
	return send(table_url(p_table, Dictionary()), HTTPClient::METHOD_POST, JSON::stringify(p_rows), more, GDSupabaseCallInternal::ACTION_NONE, GDSupabaseCallInternal::SHAPE_ARRAY);
}

// filterに合う行を更新する。
Signal GDSupabaseClient::update(const String &p_table, const Dictionary &p_values, const Dictionary &p_filters) {
	Dictionary more;
	more["Prefer"] = "return=representation";
	return send(table_url(p_table, p_filters), HTTPClient::METHOD_PATCH, JSON::stringify(p_values), more, GDSupabaseCallInternal::ACTION_NONE, GDSupabaseCallInternal::SHAPE_ARRAY);
}

// filterに合う行を削除する。
Signal GDSupabaseClient::remove(const String &p_table, const Dictionary &p_filters) {
	Dictionary more;
	more["Prefer"] = "return=representation";
	return send(table_url(p_table, p_filters), HTTPClient::METHOD_DELETE, String(), more, GDSupabaseCallInternal::ACTION_NONE, GDSupabaseCallInternal::SHAPE_ARRAY);
}

// PostgreSQL functionを呼ぶ。
Signal GDSupabaseClient::rpc(const String &p_function, const Dictionary &p_args) {
	ERR_FAIL_COND_V_MSG(!safe_name(p_function), Signal(), "function must be an ASCII identifier");
	return send(base_url + "/rest/v1/rpc/" + p_function, HTTPClient::METHOD_POST, JSON::stringify(p_args), Dictionary(), GDSupabaseCallInternal::ACTION_NONE, GDSupabaseCallInternal::SHAPE_VARIANT);
}

// emailとpasswordでAuth sessionを得る。
Signal GDSupabaseClient::sign_in(const String &p_email, const String &p_password) {
	Dictionary body;
	body["email"] = p_email;
	body["password"] = p_password;
	return send(base_url + "/auth/v1/token?grant_type=password", HTTPClient::METHOD_POST, JSON::stringify(body), Dictionary(), GDSupabaseCallInternal::ACTION_SESSION, GDSupabaseCallInternal::SHAPE_DICTIONARY, false);
}

// projectの公開Auth設定を得る。
Signal GDSupabaseClient::auth_settings() {
	return send(base_url + "/auth/v1/settings", HTTPClient::METHOD_GET, String(), Dictionary(), GDSupabaseCallInternal::ACTION_NONE, GDSupabaseCallInternal::SHAPE_DICTIONARY, false);
}

// refresh tokenでAuth sessionを更新する。
Signal GDSupabaseClient::refresh(const String &p_refresh_token) {
	const String token = p_refresh_token.is_empty() ? String(auth_session.get("refresh_token", "")) : p_refresh_token;
	Dictionary body;
	body["refresh_token"] = token;
	return send(base_url + "/auth/v1/token?grant_type=refresh_token", HTTPClient::METHOD_POST, JSON::stringify(body), Dictionary(), GDSupabaseCallInternal::ACTION_SESSION, GDSupabaseCallInternal::SHAPE_DICTIONARY, false);
}

// server側のsessionを無効化する。
Signal GDSupabaseClient::sign_out() {
	return send(base_url + "/auth/v1/logout", HTTPClient::METHOD_POST, "{}", Dictionary(), GDSupabaseCallInternal::ACTION_SIGN_OUT, GDSupabaseCallInternal::SHAPE_VARIANT, false);
}

// 外から渡されたsessionを現在値にする。
void GDSupabaseClient::set_session(const Dictionary &p_session) {
	auth_session = p_session;
}

// 現在のsessionを返す。
Dictionary GDSupabaseClient::get_session() const {
	return auth_session.duplicate();
}

// projectの基準URLを返す。
String GDSupabaseClient::get_url() const {
	return base_url;
}

// Supabase clientの公開methodを登録する。通常名は自動で待ち、_async名はGDTaskを返す（manifestの[await]に両方を書く）。
void GDSupabaseClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("select", "table", "query"), &GDSupabaseClient::select, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("select_async", "table", "query"), &GDSupabaseClient::select, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("insert", "table", "rows", "upsert"), &GDSupabaseClient::insert, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("insert_async", "table", "rows", "upsert"), &GDSupabaseClient::insert, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("update", "table", "values", "filters"), &GDSupabaseClient::update);
	ClassDB::bind_method(D_METHOD("update_async", "table", "values", "filters"), &GDSupabaseClient::update);
	ClassDB::bind_method(D_METHOD("remove", "table", "filters"), &GDSupabaseClient::remove);
	ClassDB::bind_method(D_METHOD("remove_async", "table", "filters"), &GDSupabaseClient::remove);
	ClassDB::bind_method(D_METHOD("rpc", "function", "args"), &GDSupabaseClient::rpc, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("rpc_async", "function", "args"), &GDSupabaseClient::rpc, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("sign_in", "email", "password"), &GDSupabaseClient::sign_in);
	ClassDB::bind_method(D_METHOD("sign_in_async", "email", "password"), &GDSupabaseClient::sign_in);
	ClassDB::bind_method(D_METHOD("auth_settings"), &GDSupabaseClient::auth_settings);
	ClassDB::bind_method(D_METHOD("auth_settings_async"), &GDSupabaseClient::auth_settings);
	ClassDB::bind_method(D_METHOD("refresh", "refresh_token"), &GDSupabaseClient::refresh, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("refresh_async", "refresh_token"), &GDSupabaseClient::refresh, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("sign_out"), &GDSupabaseClient::sign_out);
	ClassDB::bind_method(D_METHOD("sign_out_async"), &GDSupabaseClient::sign_out);
	ClassDB::bind_method(D_METHOD("set_session", "session"), &GDSupabaseClient::set_session);
	ClassDB::bind_method(D_METHOD("get_session"), &GDSupabaseClient::get_session);
	ClassDB::bind_method(D_METHOD("get_url"), &GDSupabaseClient::get_url);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "session"), "set_session", "get_session");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "", "get_url");
}

// 設定済みclientを作る。
Ref<GDSupabaseClient> GDSupabase::client(const String &p_url, const String &p_key, const Dictionary &p_opts) {
	const String url = p_url.trim_suffix("/");
	const bool https = url.begins_with("https://");
	const bool http = url.begins_with("http://");
	const String authority = url.get_slice("://", 1).get_slice("/", 0);
	const bool local = http && (authority == "127.0.0.1" || authority.begins_with("127.0.0.1:") || authority == "localhost" || authority.begins_with("localhost:"));
	const bool clean = !authority.is_empty() && !authority.contains("@") && !url.contains("?") && !url.contains("#") &&
			url == String(https ? "https://" : "http://") + authority;
	ERR_FAIL_COND_V_MSG((!https && !local) || !clean, Ref<GDSupabaseClient>(), "Supabase URL must be an HTTPS origin or localhost");
	ERR_FAIL_COND_V_MSG(p_key.is_empty(), Ref<GDSupabaseClient>(), "Supabase publishable key is required");
	ERR_FAIL_COND_V_MSG(p_key.begins_with("sb_secret_") || is_legacy_secret(p_key), Ref<GDSupabaseClient>(), "Do not use a Supabase secret key in this client");
	const String schema = p_opts.get("schema", "public");
	ERR_FAIL_COND_V_MSG(!GDSupabaseClient::safe_name(schema), Ref<GDSupabaseClient>(), "schema must be an ASCII identifier");
	Ref<GDSupabaseClient> out;
	out.instantiate();
	out->setup(url, p_key, p_opts);
	return out;
}

// Supabase Singletonのfactoryを登録する。
void GDSupabase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("client", "url", "publishable_key", "opts"), &GDSupabase::client, DEFVAL(Dictionary()));
}

} // namespace godot
