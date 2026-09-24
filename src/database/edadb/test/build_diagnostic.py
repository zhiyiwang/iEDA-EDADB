"""Relink current Release objects with adapter logging, without changing production outputs."""
import concurrent.futures
import pathlib
import shlex
import shutil
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[4]
build = root / "build-release"
output = pathlib.Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=False)
flags = {}
for line in (build / "src/database/manager/builder/def_builder/CMakeFiles/def_builder.dir/flags.make").read_text().splitlines():
    if line.startswith("CXX_") and " = " in line:
        name, value = line.split(" = ", 1)
        flags[name] = shlex.split(value)


def compile_source(name):
    subprocess.run(["/usr/bin/g++-10", *flags["CXX_DEFINES"], *flags["CXX_INCLUDES"],
                    *flags["CXX_FLAGS"], "-DEDADB_OUTPUT_DEBUG=1", "-c",
                    str(root / "src/database/manager/builder/def_builder" / name),
                    "-o", str(output / (name + ".o"))], check=True)


# Only two translation units need rebuilding; all other Release objects are reused.
names = ["def_read_edadb.cpp", "def_write_edadb.cpp"]
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(compile_source, names))
archive = output / "libdef_builder.a"
shutil.copy2(build / "lib/libdef_builder.a", archive)
subprocess.run(["/usr/bin/gcc-ar-10", "r", str(archive),
                *(str(output / (name + ".o")) for name in names)], check=True)
subprocess.run(["/usr/bin/gcc-ranlib-10", str(archive)], check=True)
command = shlex.split((build / "src/apps/CMakeFiles/iEDA.dir/link.txt").read_text())
command[command.index("-o") + 1] = str(output / "iEDA")
command = [str(archive) if value.endswith("/libdef_builder.a") else value for value in command]
subprocess.run(command, cwd=build / "src/apps", check=True)
