"""Deterministic 8-field fixtures and SHA256 helper."""
import hashlib
from pathlib import Path

def sha(path):
    with path.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256") if hasattr(hashlib, "file_digest") else None
        if digest is not None:
            return digest.hexdigest()
        checksum = hashlib.sha256()
        for chunk in iter(lambda: source.read(1048576), b""):
            checksum.update(chunk)
        return checksum.hexdigest()

def prepare(directory, count):
    directory.mkdir()
    (directory/"bench.lef").write_text("""VERSION 5.8 ;
BUSBITCHARS "[]" ;
DIVIDERCHAR "/" ;
UNITS
 DATABASE MICRONS 1000 ;
END UNITS
MANUFACTURINGGRID 0.001 ;
SITE bench_site
 CLASS CORE ;
 SIZE 0.01 BY 0.01 ;
 SYMMETRY X Y ;
END bench_site
MACRO bench_cell
 CLASS CORE ;
 ORIGIN 0 0 ;
 SIZE 0.01 BY 0.01 ;
 SYMMETRY X Y ;
 SITE bench_site ;
END bench_cell
END LIBRARY
""")
    with (directory/"canonical.def").open("w") as output:
        output.write('VERSION 5.8 ;\nDIVIDERCHAR "/" ;\nBUSBITCHARS "[]" ;\nDESIGN sqlite_baseline ;\n'
                     'UNITS DISTANCE MICRONS 1000 ;\nDIEAREA ( 0 0 ) ( 200000 200000 ) ;\n')
        output.write(f"COMPONENTS {count} ;\n")
        for index in range(count):
            output.write(f"- U{index:07d} bench_cell + SOURCE DIST + PLACED ( {index%1000*100} {index//1000*100} ) N ;\n")
        output.write("END COMPONENTS\nEND DESIGN\n")
