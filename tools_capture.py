import ctypes, os, subprocess, struct, time
env = dict(os.environ, GRID_RESOURCE_DIR='E:/infinite-grid')
proc = subprocess.Popen(['./WINDOW.exe'], env=env, cwd='E:/infinite-grid/build2022/bin/Debug',
                        stdout=open('imgui_smoke7.log', 'wb'), stderr=subprocess.STDOUT)
time.sleep(6)
user32 = ctypes.windll.user32
pid = None
out = subprocess.check_output(
    ['tasklist', '/FI', 'IMAGENAME eq WINDOW.exe', '/FO', 'CSV']).decode('gbk', 'ignore')
for line in out.splitlines():
    if 'WINDOW.exe' in line:
        pid = int(line.split('","')[1])
        break
print('pid:', pid)
find = None
if pid:
    CF = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    def cb(hwnd, lp):
        global find
        wpid = ctypes.c_uint32()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid and user32.IsWindowVisible(hwnd):
            buf = ctypes.create_unicode_buffer(160)
            user32.GetWindowTextW(hwnd, buf, 160)
            if buf.value:
                find = hwnd
                print('title:', buf.value)
        return True
    user32.EnumWindows(CF(cb), None)
if find:
    user32.SetForegroundWindow(find)
    time.sleep(0.4)
    class RECT(ctypes.Structure):
        _fields_ = [('l', ctypes.c_long), ('t', ctypes.c_long),
                    ('r', ctypes.c_long), ('b', ctypes.c_long)]
    rect = RECT()
    user32.GetWindowRect(find, ctypes.byref(rect))
    w, h = rect.r - rect.l, rect.b - rect.t
    hdc = user32.GetWindowDC(find)
    mem = ctypes.windll.gdi32.CreateCompatibleDC(hdc)
    bmp = ctypes.windll.gdi32.CreateCompatibleBitmap(hdc, w, h)
    ctypes.windll.gdi32.SelectObject(mem, bmp)
    user32.PrintWindow(find, mem, 2)
    class BI(ctypes.Structure):
        _fields_ = [('biSize', ctypes.c_uint32), ('biWidth', ctypes.c_int32),
                    ('biHeight', ctypes.c_int32), ('biPlanes', ctypes.c_uint16),
                    ('biBitCount', ctypes.c_uint16), ('biCompression', ctypes.c_uint32),
                    ('biSizeImage', ctypes.c_uint32), ('biXPels', ctypes.c_int32),
                    ('biYPels', ctypes.c_int32), ('biClrUsed', ctypes.c_uint32),
                    ('biClrImportant', ctypes.c_uint32)]
    bi = BI(ctypes.sizeof(BI), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * h * 4)
    ctypes.windll.gdi32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), 0)
    open('imgui_panel_check.bmp', 'wb').write(
        b'BM' + struct.pack('<IHHI', 54 + w * h * 4, 0, 0, 54) +
        struct.pack('<IiiHHIIiiII', 40, w, -h, 1, 32, 0, w * h * 4, 0, 0, 0, 0) +
        buf.raw)
    from PIL import Image
    Image.open('imgui_panel_check.bmp').save('imgui_panel_check.png')
    print('captured', w, h)
else:
    print('window not found')
proc.kill()
