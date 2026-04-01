import json, struct, re

INPUT = "std_config.json"
OUT_BIN = "std_config.bin"

def to_uint8(v):
    if isinstance(v, int):
        n = (v+48) & 0xFF
        ret = (n, None)

    elif isinstance(v, str):
        s = v.strip()
        port_descriptor = ord(s[0]) & 0xFF
        number_descriptor = ord(s[1]) & 0xFF
        ret = (port_descriptor, number_descriptor)

    return ret


if __name__ == "__main__":
    with open(INPUT, "r", encoding="utf-8") as f:
        data = json.load(f)

    out = bytearray()

    hi, lo = to_uint8(data["id"])
    if(lo == None):
        out += struct.pack("B", hi)

    converted = {}  # para mostrar no terminal
    for i in range(1, 37):

        pair = data[f"led_{i}"]
        if not isinstance(pair, (list, tuple)) or len(pair) != 2:
            raise ValueError(f"led_{i} deve ser lista com 2 elementos")
        a = to_uint8(pair[0])

        hi_a, lo_a = to_uint8(pair[0])
        out += struct.pack("BB", hi_a, lo_a)

        hi_b, lo_b = to_uint8(pair[1])
        out += struct.pack("BB", hi_b, lo_b)
    
        converted[f"led_{i}"] = (hi_a, lo_a, hi_b, lo_b)

    # grava bin e hex
    with open(OUT_BIN, "wb") as f:
        f.write(out)

    # imprime resumo
    for k, (a,b,c,d) in converted.items():
        print(f"{k}: {a:02X} {b:02X} {c:02X} {d:02X} ")
    print(f"\nWrote {len(out)} bytes -> {OUT_BIN}")
