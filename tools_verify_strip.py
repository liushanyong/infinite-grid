import ctypes, os, subprocess, struct, time
import ctypes.wintypes
from PIL import Image, ImageGrab
import numpy as np

env = dict(os.environ, GRID_RESOURCE_DIR='E:/infinite-grid')
proc = subprocess.Popen(['E:/infinite-grid/build2022/bin/Debug/WINDOW.exe'], env=env,
                        cwd='E:/infinite-grid/build2022/bin/Debug',
                        stdout=open('strip_verify.log', 'wb'),
                        stderr=subprocess.STDOUT)

user32 = ctypes.windll.user32
# physical-pixel coordinates for everything below
ctypes.windll.shcore.SetProcessDpiAwareness(2)

class RECT(ctypes.Structure):
    _fields_ = [('l', ctypes.c_long), ('t', ctypes.c_long),
                ('r', ctypes.c_long), ('b', ctypes.c_long)]

def find_window():
    out = subprocess.check_output(
        ['tasklist', '/FI', 'IMAGENAME eq WINDOW.exe', '/FO', 'CSV']).decode('gbk', 'ignore')
    pid = None
    for line in out.splitlines():
        if 'WINDOW.exe' in line:
            pid = int(line.split('","')[1])
            break
    if not pid:
        return None, None
    CF = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    found = []
    def cb(hwnd, lp):
        wpid = ctypes.c_uint32()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid and user32.IsWindowVisible(hwnd):
            buf = ctypes.create_unicode_buffer(160)
            user32.GetWindowTextW(hwnd, buf, 160)
            if buf.value:
                found.append((hwnd, buf.value))
        return True
    user32.EnumWindows(CF(cb), None)
    for hwnd, title in found:
        if 'grid plane' in title:
            return hwnd, title
    return (found[0] if found else (None, None))

def capture(tag):
    hwnd, title = find_window()
    if not hwnd:
        print(tag, 'window not found')
        return None, None, None
    print(tag, 'captured title:', title)
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.5)
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.r - rect.l, rect.b - rect.t
    client = RECT()
    user32.GetClientRect(hwnd, ctypes.byref(client))
    pt = ctypes.wintypes.POINT(client.l, client.t)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    client_off = (pt.x - rect.l, pt.y - rect.t)   # title+frame size
    client_size = (client.r, client.b)
    path = f'strip_verify_{tag}.png'
    ImageGrab.grab(bbox=(rect.l, rect.t, rect.r, rect.b)).save(path)
    return path, client_off, client_size

time.sleep(5)
before, off_b, size_b = capture('before')
# real middle-button drag = orbit (drives the camera like a user would)
hwnd_b, _ = find_window()
ctypes.windll.user32.SwitchToThisWindow(hwnd_b, True)
time.sleep(0.5)
fg = user32.GetForegroundWindow()
print('focus ok:', fg == hwnd_b)
rect0 = RECT()
user32.GetWindowRect(hwnd_b, ctypes.byref(rect0))
cx, cy = rect0.l + 300, rect0.t + 400
user32.SetCursorPos(cx, cy)
time.sleep(0.2)
user32.mouse_event(0x20, 0, 0, 0, 0)   # middle down
for i in range(12):
    user32.mouse_event(0x0001, 20, 8, 0, 0)  # relative move
    time.sleep(0.03)
user32.mouse_event(0x40, 0, 0, 0, 0)   # middle up
time.sleep(1.5)
after, off_a, size_a = capture('after')
proc.kill()

if before and after and size_b == size_a:
    a = Image.open(before).convert('RGB')
    b = Image.open(after).convert('RGB')
    w, h = a.size
    cw, ch = size_b
    ox, oy = off_b
    print('window size:', w, h, 'client:', cw, ch, 'client offset:', ox, oy)
    na, nb = np.asarray(a, dtype=int), np.asarray(b, dtype=int)
    for label, y0, y1 in [('bottom24 (old artifact band)', ch - 26, ch - 2),
                          ('just above old strip', ch - 34, ch - 26),
                          ('mid client', ch // 2 - 8, ch // 2 + 8)]:
        ba = na[oy + y0:oy + y1, ox:ox + cw]
        bb = nb[oy + y0:oy + y1, ox:ox + cw]
        changed = (abs(ba - bb).sum(axis=2) > 12).mean()
        rows = ba.reshape(-1, 3)
        print(f'{label}: change={changed:.3f} before_sample={rows[::max(1, len(rows)//4)][:4].tolist()}')
else:
    print('capture failed or client size mismatch', size_b, size_a)
