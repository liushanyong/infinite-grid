import ctypes, os, subprocess, sys, time
import ctypes.wintypes
from PIL import Image, ImageGrab
import numpy as np

EXE = 'E:/infinite-grid/build2022/bin/Debug/WINDOW.exe'
env = dict(os.environ, GRID_RESOURCE_DIR='E:/infinite-grid')
proc = subprocess.Popen([EXE], env=env,
                        cwd='E:/infinite-grid/build2022/bin/Debug',
                        stdout=open('vstyle.log', 'wb'), stderr=subprocess.STDOUT)

user32 = ctypes.windll.user32
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
            pid = int(line.split('","')[1]); break
    if not pid: return None
    CF = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    found = []
    def cb(hwnd, lp):
        wpid = ctypes.c_uint32()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid and user32.IsWindowVisible(hwnd):
            buf = ctypes.create_unicode_buffer(160)
            user32.GetWindowTextW(hwnd, buf, 160)
            if 'grid plane' in buf.value: found.append(hwnd)
        return True
    user32.EnumWindows(CF(cb), None)
    return found[0] if found else None

def grab(tag, hwnd):
    r = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    ImageGrab.grab(bbox=(r.l, r.t, r.r, r.b)).save(f'vstyle_{tag}.png')

def send_key(vk):
    scan = user32.MapVirtualKeyW(vk, 0)
    user32.keybd_event(vk, scan, 0, 0); time.sleep(0.05)
    user32.keybd_event(vk, scan, 2, 0)

time.sleep(5)
hwnd = find_window()
ctypes.windll.user32.SwitchToThisWindow(hwnd, True)
time.sleep(0.6)
grab('t0', hwnd)
send_key(0x56); time.sleep(6.0); grab('t1', hwnd)
send_key(0x56); time.sleep(6.0); grab('t2', hwnd)
proc.kill()

a = np.asarray(Image.open('vstyle_t0.png').convert('RGB'), dtype=int)
b = np.asarray(Image.open('vstyle_t1.png').convert('RGB'), dtype=int)
c = np.asarray(Image.open('vstyle_t2.png').convert('RGB'), dtype=int)
print('t1 vs t0:', round((abs(a-b).sum(axis=2) > 12).mean(), 4))
print('t2 vs t1:', round((abs(b-c).sum(axis=2) > 12).mean(), 4))
print('t2 vs t0:', round((abs(a-c).sum(axis=2) > 12).mean(), 4))
