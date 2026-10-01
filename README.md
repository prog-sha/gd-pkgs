# gd packages

[日本語](README.ja.md)

Packages **0.8.0** for **GD 0.8** and **Godot 4.7.2**. Source repository: [prog-sha/gd-pkgs](https://github.com/prog-sha/gd-pkgs).

Install them from the registry `https://gd.progsha.com/pkg`:

```sh
gd install hello gd:@gd/hello@0.8.0
gd install discord gd:@gd/discord@0.8.0
gd install ext:@gd/supabase@0.8.0
```

Pure GDScript packages are read with `@import`. Native extensions are used through the class names in the manifest. From upstream Godot, set `"place": "project"` in `gd.json` and load `pkg/<alias>/`, for example `const Hello := preload("res://pkg/hello/mod.gd")`. Pin every dependency version in `gd.lock`.

| package | entry | role |
|---|---|---|
| `@gd/hello` | `@import hello` | smallest pure GDScript package |
| `@gd/discord` | `@import discord` | Discord text bot |
| `@gd/supabase` | `GDSupabase` | Supabase Database and Auth |

REST results are dictionaries `{ok, status, data, error, kind, headers}`. `kind` is a string spelled like the current `Err` category name. It is the category spelling, and the language `Err` pair is a separate value. Details are in each package note (Japanese).

- [Hello](extensions/hello/README.md)
- [Discord](extensions/discord/README.md)
- [Supabase](extensions/supabase/README.md)

## Runtimes

- Godot 4.7.2
- GD 0.8.0
- macOS arm64
- Linux x86_64
- Windows x86_64

A native extension runs with the same privileges as the process. Pin trusted packages and versions in `gd.lock` and commit that file.

Creating, building, and publishing packages: [Authoring packages](AUTHORING.md) (Japanese). Publishing to the official registry requires permission for that scope.
