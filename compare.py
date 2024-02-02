with open("chroma74.bin", "rb") as f1:
    with open("last_compressed_img.bin", "rb") as f2:
        orig = f2.read()
        c = f1.read()

        if len(orig) != len(c):
            print("length does not match")

        for (idx, (a, b)) in enumerate(zip(orig, c)):
            if a != b:
                print(f"{idx}: {hex(a)} != {hex(b)}")
            else:
                print(f"{idx}: {hex(a)} == {hex(b)}")
        print(orig == c)
