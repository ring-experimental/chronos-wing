import sys, zipfile
src, dst, size = sys.argv[1:4]
with zipfile.ZipFile(src) as zi, zipfile.ZipFile(dst, 'w', zipfile.ZIP_STORED) as zo:
    for it in zi.infolist():
        data = zi.read(it.filename)
        if it.filename == '__adf__':
            assert b'DisplaySize:240*320' in data
            data = data.replace(b'DisplaySize:240*320', b'DisplaySize:' + size.encode())
        zo.writestr(it.filename, data)
