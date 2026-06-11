# PlatformIO pre-build script: reads AP_SSID / AP_PASS from the repo-root
# .env (or .env.dev as fallback) and injects them as compile-time defines,
# so personal credentials never need to be edited into main.cpp.
Import("env")
import os

root = os.path.dirname(env["PROJECT_DIR"])
path = None
for name in (".env", ".env.dev"):
    candidate = os.path.join(root, name)
    if os.path.isfile(candidate):
        path = candidate
        break

if path:
    defines = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key, value = key.strip(), value.strip().strip('"').strip("'")
            if key in ("AP_SSID", "AP_PASS") and value:
                defines.append((key, env.StringifyMacro(value)))
    if defines:
        env.Append(CPPDEFINES=defines)
        print(
            "load_env.py: %s -> %s"
            % (os.path.basename(path), ", ".join(k for k, _ in defines))
        )
