import ctypes, os, subprocess, sys, time
import ctypes.wintypes
from PIL import Image, ImageGrab
import numpy as np

EXE = sys.argv[1] if len(sys.argv) > 1 else 'E:/infinite-grid/build2022/bin/Debug/WINDOW.exe'
TAG = sys.argv[2] if len(sys.argv) > 2 else 'new'
env = dict(os.environ,
           GRID_RESOURCE_DIR='E:/infinite-grid',
           GRID_PICK_DEBUG='1')
proc = subprocess.Popen([EXE], env=env,
                        cwd='E:/infinite-grid/build2022/bin/Debug',
                        stdout=open(f'regress_{TAG}.log', 'wb'),
                        stderr=subprocess.STDOUT)

user32 = ctypes.windll.user32
ctypes.windll.shcore.SetProcessDpiAwareness(2)

class RECT(ctypes.Structure):
    _fields_ = [('l', ctypes.c_long), ('t', ctypes.c_long),
                ('r', ctypes.c_long), ('b', ctypes.c_long)]

def find_window():
    image = os.path.basename(EXE)
    out = subprocess.check_output(
        ['tasklist', '/FI', f'IMAGENAME eq {image}', '/FO', 'CSV']).decode('gbk', 'ignore')
    pid = None
    for line in out.splitlines():
        if image in line:
            pid = int(line.split('","')[1])
            break
    if not pid:
        return None
    CF = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    found = []
    def cb(hwnd, lp):
        wpid = ctypes.c_uint32()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid and user32.IsWindowVisible(hwnd):
            buf = ctypes.create_unicode_buffer(160)
            user32.GetWindowTextW(hwnd, buf, 160)
            if 'grid plane' in buf.value:
                found.append(hwnd)
        return True
    user32.EnumWindows(CF(cb), None)
    return found[0] if found else None

def grab(tag, hwnd):
    r = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    path = f'regress_{TAG}_{tag}.png'
    ImageGrab.grab(bbox=(r.l, r.t, r.r, r.b)).save(path)
    return path

def send_key(vk):
    scan = user32.MapVirtualKeyW(vk, 0)
    user32.keybd_event(vk, scan, 0, 0)
    time.sleep(0.05)
    user32.keybd_event(vk, scan, 2, 0)

def click(x, y):
    user32.SetCursorPos(x, y)
    time.sleep(0.15)
    user32.mouse_event(0x02, 0, 0, 0, 0)   # left down
    time.sleep(0.05)
    user32.mouse_event(0x04, 0, 0, 0, 0)   # left up

time.sleep(5)
hwnd = find_window()
print('hwnd:', hwnd)
if not hwnd:
    proc.kill(); raise SystemExit('window not found')
ctypes.windll.user32.SwitchToThisWindow(hwnd, True)
time.sleep(0.6)
print('focus ok:', user32.GetForegroundWindow() == hwnd)

r = RECT()
user32.GetWindowRect(hwnd, ctypes.byref(r))
cl = RECT()
user32.GetClientRect(hwnd, ctypes.byref(cl))
pt = ctypes.wintypes.POINT(0, 0)
user32.ClientToScreen(hwnd, ctypes.byref(pt))
ox, oy = pt.x - r.l, pt.y - r.t
print('client origin in window:', ox, oy, 'client size:', cl.r, cl.b)

p0 = grab('t0_startup', hwnd)

# 1. left click on a stress-field mesh (dense cluster, right side)
click(r.l + ox + 620, r.t + oy + 430)
time.sleep(2.0)
p1 = grab('t1_after_pick', hwnd)

# 2. V key = cycle visual style
send_key(0x56)
time.sleep(1.5)
p2 = grab('t2_after_vkey', hwnd)

# 3. middle drag = orbit
user32.SetCursorPos(r.l + ox + 400, r.t + oy + 400)
time.sleep(0.2)
user32.mouse_event(0x20, 0, 0, 0, 0)
for i in range(10):
    user32.mouse_event(0x0001, 25, 0, 0, 0)
    time.sleep(0.03)
user32.mouse_event(0x40, 0, 0, 0, 0)
time.sleep(1.5)
p3 = grab('t3_after_drag', hwnd)
proc.kill()

imgs = {t: np.asarray(Image.open(p).convert('RGB'), dtype=int)
        for t, p in [('t1', p1), ('t2', p2), ('t3', p3)]}
n0 = np.asarray(Image.open(p0).convert('RGB'), dtype=int)
for t, n in imgs.items():
    d = (abs(n0 - n).sum(axis=2) > 12).mean()
    print(f'{t} vs t0 diff ratio: {d:.3f}')
