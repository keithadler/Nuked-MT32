import sys, zlib, struct
def read_ppm(p):
    d = open(p,'rb').read()
    assert d[:2] == b'P6'
    i = 2; vals = []
    while len(vals) < 3:
        while d[i:i+1].isspace(): i += 1
        if d[i:i+1] == b'#':
            while d[i:i+1] != b'\n': i += 1
            continue
        s = i
        while not d[i:i+1].isspace(): i += 1
        vals.append(int(d[s:i]))
    i += 1
    w, h, mx = vals
    return w, h, d[i:i+w*h*3]
def write_png(p, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def chunk(t, data):
        c = t + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9))
    png += chunk(b'IEND', b'')
    open(p,'wb').write(png)
if __name__ == '__main__':
    w, h, rgb = read_ppm(sys.argv[1])
    write_png(sys.argv[2], w, h, rgb)
    print("wrote", sys.argv[2], w, "x", h)
