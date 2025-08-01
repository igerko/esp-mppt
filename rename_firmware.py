Import("env")
import os
import re
import shutil

def after_build(source, target, env):
    # Získaj verziu z build flags
    flags = env.ParseFlags(env['BUILD_FLAGS'])
    version = None

    for flag in flags['CPPDEFINES']:
        if isinstance(flag, (list, tuple)) and flag[0] == 'MPPT_FIRMWARE_VERSION':
            version = flag[1].strip('"')

    if not version:
        print("[ERROR] MPPT_FIRMWARE_VERSION not found in build flags.")
        return

    # Cesty
    build_dir = env.subst("$BUILD_DIR")
    src = os.path.join(build_dir, "firmware.bin")
    dst_dir = os.path.join("build_output")
    dst = os.path.join(dst_dir, f"firmware_{version}.bin")

    if not os.path.exists(src):
        print(f"[ERROR] Firmware not found: {src}")
        return

    os.makedirs(dst_dir, exist_ok=True)
    shutil.copyfile(src, dst)
    print(f"[INFO] Copied firmware to: {dst}")

# Zaregistruj callback PO vytvorení firmware.bin
env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)