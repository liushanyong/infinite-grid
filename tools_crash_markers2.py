p = 'main.cpp'
with open(p, 'rb') as f:
    raw = f.read()

nl = b'\r\n' if b'\r\n' in raw[:4000] else b'\n'
L = lambda ls: nl.join(ls)

def sub(old, new, label):
    global raw
    for v_old, v_new in ((old, new), (old.replace(b'\n', b'\r\n'),
                                     new.replace(b'\n', b'\r\n'))):
        if v_old in raw:
            assert raw.count(v_old) == 1, label
            raw = raw.replace(v_old, v_new, 1)
            print(label, 'ok')
            return
    raise AssertionError(label)

old = L([
b'      drawViewCubeOverlay();',
b'      ImGui::Render();',
b'      imguiBgfxRenderDrawData(ImGui::GetDrawData(), 255);'])
new = L([
b'      fprintf(stderr, "[T] vc enter\\n");',
b'      drawViewCubeOverlay();',
b'      fprintf(stderr, "[T] vc ok / render\\n");',
b'      ImGui::Render();',
b'      fprintf(stderr, "[T] bgfx ui\\n");',
b'      imguiBgfxRenderDrawData(ImGui::GetDrawData(), 255);',
b'      fprintf(stderr, "[T] bgfx ui ok\\n");'])
sub(old, new, 'ui markers')

old = L([
b'    acgs::acgsGetManager()->endFrame();',
b'    // Display stage: the offscreen product becomes the window image',
b'    // (ImGui panels in ImGui mode, backbuffer resolve otherwise).',
b'    acgs::acgsGetManager()->compositeFrame();'])
new = L([
b'    fprintf(stderr, "[T] endFrame enter\\n");',
b'    acgs::acgsGetManager()->endFrame();',
b'    fprintf(stderr, "[T] endFrame ok\\n");',
b'    acgs::acgsGetManager()->compositeFrame();',
b'    fprintf(stderr, "[T] composite ok\\n");'])
sub(old, new, 'frame markers')

with open(p, 'wb') as f:
    f.write(raw)
print('markers done')
