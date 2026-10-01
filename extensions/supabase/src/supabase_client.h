/**************************************************************************/
/*  supabase_client.h                                                     */
/**************************************************************************/
/*                          gd packages                                  */
/**************************************************************************/

#pragma once

// SupabaseのREST APIとAuth APIを、gdの通常名（自動待ち）と_async名（GDTask）で提供する。
// 通信はgdのGD.httpを使うので、SceneTreeの無いgd serveでも動き、gdの通信許可に従う。
// GDScriptからはSupabase Singletonのclient()で受け取り、型名は書かない。

#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/signal.hpp>

namespace godot {

class GDSupabaseClient;

// HTTP要求1件を完了まで生かし、値とErrの二値をSignalへ流す。
class GDSupabaseCallInternal : public RefCounted {
	GDCLASS(GDSupabaseCallInternal, RefCounted);

public:
	// Auth応答をclientへ反映する種類。
	enum Action {
		ACTION_NONE,
		ACTION_SESSION,
		ACTION_SIGN_OUT,
	};
	// 失敗時にも型どおりに返す空の値の種類。
	enum Shape {
		SHAPE_VARIANT,
		SHAPE_ARRAY,
		SHAPE_DICTIONARY,
	};

private:
	Ref<GDSupabaseCallInternal> self_hold; // 完了まで自分を生かす参照
	Ref<GDSupabaseClient> client; // sessionを更新するclient
	Signal fetching; // 取り消しを伝えるGD.httpの通信
	Action action = ACTION_NONE; // 完了時に行うsession操作
	Shape shape = SHAPE_VARIANT; // 失敗時に返す空の値の型

	// GD.httpの応答を値とErrへ直す。
	void completed(const Variant &p_response, const Variant &p_error);
	// 開始前の失敗を次のturnでSignalへ流す。
	void failed(const String &p_message);
	// 値とErrを通知し、自己参照を片付ける。
	void finish(const Variant &p_value, const Variant &p_error);
	// 失敗時に返す空の値を作る。
	Variant empty() const;

protected:
	// 通信CallのmethodとSignalを登録する。
	static void _bind_methods();

public:
	// GD.httpで通信を始める。
	void start(const Ref<GDSupabaseCallInternal> &p_self, const Ref<GDSupabaseClient> &p_client,
			const String &p_url, HTTPClient::Method p_method, const Dictionary &p_headers,
			const String &p_body, Action p_action, Shape p_shape);
	// 通信を取り消す。結果は中断のErrとして届く。
	void cancel();
};

// project設定、session、Database/Authの短いAPIを持つ内部client。
class GDSupabaseClient : public RefCounted {
	GDCLASS(GDSupabaseClient, RefCounted);

	String base_url; // projectの基準URL
	String api_key; // publishableまたは旧anon key
	String schema = "public"; // PostgRESTへ渡すschema
	Dictionary extra_headers; // 全要求へ足す見出し
	Dictionary auth_session; // Authが返した現在のsession

	// table、function、schemaへ使えるASCII識別子か調べる。
	static bool safe_name(const String &p_name);
	// query値をpercent encodeしてURLへ繋ぐ。
	static String query_text(const Dictionary &p_query);
	// 共通認証とschemaの見出しを組む。
	Dictionary headers(bool p_json, bool p_mutation, bool p_rest, const Dictionary &p_more = Dictionary()) const;
	// REST tableのURLを安全な名前から組む。
	String table_url(const String &p_table, const Dictionary &p_query) const;
	// HTTP Callを作り、待てるSignalを返す。
	Signal send(const String &p_url, HTTPClient::Method p_method, const String &p_body, const Dictionary &p_more,
			GDSupabaseCallInternal::Action p_action, GDSupabaseCallInternal::Shape p_shape, bool p_rest = true);
	// Auth成功時に新しいsessionを控える。
	void accept_session(const Dictionary &p_session);
	// sign out成功時にsessionを空にする。
	void clear_session();

	friend class GDSupabaseCallInternal;
	friend class GDSupabase;

protected:
	// Supabase clientの公開methodを登録する。
	static void _bind_methods();

public:
	// project URL、key、schema等を設定する。
	void setup(const String &p_url, const String &p_key, const Dictionary &p_opts);
	// tableから行を選ぶ。
	Signal select(const String &p_table, const Dictionary &p_query = Dictionary());
	// tableへ行を追加する。
	Signal insert(const String &p_table, const Variant &p_rows, bool p_upsert = false);
	// filterに合う行を更新する。
	Signal update(const String &p_table, const Dictionary &p_values, const Dictionary &p_filters);
	// filterに合う行を削除する。
	Signal remove(const String &p_table, const Dictionary &p_filters);
	// PostgreSQL functionを呼ぶ。
	Signal rpc(const String &p_function, const Dictionary &p_args = Dictionary());
	// emailとpasswordでAuth sessionを得る。
	Signal sign_in(const String &p_email, const String &p_password);
	// projectの公開Auth設定を得る。
	Signal auth_settings();
	// refresh tokenでAuth sessionを更新する。
	Signal refresh(const String &p_refresh_token = String());
	// server側のsessionを無効化する。
	Signal sign_out();
	// 外から渡されたsessionを現在値にする。
	void set_session(const Dictionary &p_session);
	// 現在のsessionを返す。
	Dictionary get_session() const;
	// projectの基準URLを返す。
	String get_url() const;
};

// Supabase clientの生成口を大域Singletonへまとめる。
class GDSupabase : public Object {
	GDCLASS(GDSupabase, Object);

protected:
	// Supabase Singletonのfactoryを登録する。
	static void _bind_methods();

public:
	// 設定済みclientを作る。
	Ref<GDSupabaseClient> client(const String &p_url, const String &p_key, const Dictionary &p_opts = Dictionary());
};

} // namespace godot
