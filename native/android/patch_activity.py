# SDLActivity.java 고치기.
# 1. 저장 파일 창의 기본 이름을 chronos-wing-YYYY-MM-DD.cwsave로 정한다.
# 2. 파일 창이 준 주소의 권한을 앱 전체 권한으로 붙잡는다. 2026-11-03 폰에서 내보내기가 Permission Denial로 막혔다.
# 3. openFileDescriptor가 FileNotFoundException 말고 다른 예외(SecurityException 등)도 잡아 -1을 돌려주고 남긴다.
#    잡지 않으면 예외가 JNI에 걸린 채 0이 돌아와 표준 입력에 쓰고 성공으로 끝날 수 있다.
import sys
p = sys.argv[1]
s = open(p).read()

def sub(a, b):
    global s
    assert s.count(a) == 1, a
    s = s.replace(a, b)

a = 'intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, allowMultiple);'
sub(a, a + """
        if (forWrite) intent.putExtra(Intent.EXTRA_TITLE, "chronos-wing-" + new java.text.SimpleDateFormat("yyyy-MM-dd", java.util.Locale.US).format(new java.util.Date()) + ".cwsave");""")

a = """                    /* Only one file is selected. */
                    filelist = new String[]{singleFileUri.toString()};"""
sub(a, a + """
                    try {
                        getContentResolver().takePersistableUriPermission(singleFileUri, data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
                    } catch (Throwable e) {
                        Log.w(TAG, "takePersistableUriPermission " + singleFileUri + ": " + e);
                    }""")

sub("""            return pfd != null ? pfd.detachFd() : -1;
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return -1;
        }""", """            return pfd != null ? pfd.detachFd() : -1;
        } catch (Throwable e) {
            Log.e(TAG, "openFileDescriptor " + uri + " " + mode + ": " + e);
            return -1;
        }""")
open(p, 'w').write(s)
