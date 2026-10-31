"""Local host-test compiler selection; no downloads, device or network access."""
from pathlib import Path
import os,shlex,shutil

def host_compiler():
    zig=os.environ.get('ZIG_EXE') or shutil.which('zig')
    if zig:return [zig,'c++']
    cxx=os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
    if cxx:return [cxx] if Path(cxx).is_file() else shlex.split(cxx)
    raise SystemExit('Set ZIG_EXE to Zig 0.14.1 or CXX to a C++17 compiler.')

def arduinojson_include():
    path=os.environ.get('ARDUINOJSON_INCLUDE')
    if not path or not (Path(path)/'ArduinoJson.h').is_file():
        raise SystemExit('Set ARDUINOJSON_INCLUDE to ArduinoJson 7.4.3 src directory.')
    return Path(path)
