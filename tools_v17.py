
p = 'main.cpp'
with open(p,'rb') as f: raw = f.read()
old = b'imguiBgfxRenderDrawData(dd, 255);'
assert raw.count(old) == 1
raw = raw.replace(old, b'imguiBgfxRenderDrawData(dd, 17);', 1)
with open(p,'wb') as f: f.write(raw)
print('view 17 test')
