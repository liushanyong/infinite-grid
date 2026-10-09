import ctypes, os, subprocess, sys, time
import ctypes.wintypes
from PIL import Image, ImageGrab

EXE = sys.argv[1]
TAG = sys.argv[2]
LOG = f'final_{TAG}.log'
env = dict(os.environ, GRID_RESOURCE_DIR='E:/infinite-grid',
           GRID_CAMERA_START_TARGET='10002142,-1152,9999072,1200',
           GRID_MULTI_VIEW='1')
proc = subprocess.Popen([EXE], env=env,
                        cwd='E:/infinite-grid/build2022/bin/Debug',
                        stdout=open(LOG, 'wb'), stderr=subprocess.STDOUT)

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

def grab(tag):
    r = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    ImageGrab.grab(bbox=(r.l, r.t, r.r, r.b)).save(f'final_{TAG}_{tag}.png')

def wait_log(marker, timeout=8.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            with open(LOG, 'rb') as f:
                if marker.encode() in f.read(): return time.time() - t0
        except OSError: pass
        time.sleep(0.1)
    return None

def send_key(vk):
    scan = user32.MapVirtualKeyW(vk, 0)
    user32.keybd_event(vk, scan, 0, 0); time.sleep(0.05)
    user32.keybd_event(vk, scan, 2, 0)

time.sleep(5)
hwnd = find_window()
ctypes.windll.user32.SwitchToThisWindow(hwnd, True)
time.sleep(0.6)
r = RECT(); user32.GetWindowRect(hwnd, ctypes.byref(r))
grab('t0_startup')
print('text visible at startup: see final_%s_t0_startup.png' % TAG)

time.sleep(3.0)
grab('t0_dual2')
proc.kill()
print('done')
