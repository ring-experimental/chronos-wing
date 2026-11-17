# mkwide.py <원본 zip> <출력 zip> <폭>: WIE로 와이드 화면을 시험하려고 원본 zip의 화면 크기 정보만 바꾼 사본을 만든다.
# 입력과 출력 모두 원본이라 저장소에 없다(work/wide/는 .gitignore).
import sys, zipfile
src, dst, size = sys.argv[1:4]
with zipfile.ZipFile(src) as zi, zipfile.ZipFile(dst, 'w', zipfile.ZIP_STORED) as zo:
    for it in zi.infolist():
        data = zi.read(it.filename)
        if it.filename == '__adf__':
            assert b'DisplaySize:240*320' in data
            data = data.replace(b'DisplaySize:240*320', b'DisplaySize:' + size.encode())
        zo.writestr(it.filename, data)
