import json, struct, re

INPUT = "wavelength_config.json"
OUT_BIN = "wavelength_config.bin"

def to_uint16(v):
    if isinstance(v, int):
        n = v & 0xFFFF
        return n

if __name__ == "__main__":
    with open(INPUT, "r", encoding="utf-8") as f:
        data = json.load(f)

    out = bytearray()

    converted = {}  # para mostrar no terminal
    for i in range(1, 37):

        wl = data[f"led_{i}"]

        ret = to_uint16(wl)
        if ret != None:
            out += struct.pack("H", ret)
        
        converted[f"led_{i}"] = (ret)

    # grava bin e hex
    with open(OUT_BIN, "wb") as f:
        f.write(out)

    # imprime resumo
    for k, l in converted.items():
        print(f"{k}: {l:04X}")
    print(f"\nWrote {len(out)} bytes -> {OUT_BIN}")
