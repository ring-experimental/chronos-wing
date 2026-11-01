# 저장 파일 창의 기본 이름을 chronos-wing-YYYY-MM-DD.cwsave로 정한다.
import sys
p = sys.argv[1]
s = open(p).read()
a = 'intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, allowMultiple);'
assert s.count(a) == 1
s = s.replace(a, a + """
        if (forWrite) intent.putExtra(Intent.EXTRA_TITLE, "chronos-wing-" + new java.text.SimpleDateFormat("yyyy-MM-dd", java.util.Locale.US).format(new java.util.Date()) + ".cwsave");""")
open(p, 'w').write(s)
