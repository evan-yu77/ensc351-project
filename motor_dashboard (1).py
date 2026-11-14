#!/usr/bin/env python3
"""ENSC 351 Motor dashboard: Python stdlib + installed libgpiod v1 + i2c-tools.

The HTTP server is for a trusted private USB network ONLY. No authentication.
Motor power must be physically disconnectable; software is not a safety interlock.
"""
import csv
import io
import json
import subprocess
import threading
import time
from collections import deque
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

import gpiod

HOST = "192.168.7.2"  # BeagleY-AI USB-network address
PORT = 5000
I2C_BUS = 1
INA219_ADDRESS = 0x40
SHUNT_OHMS = 0.01  # ASSUMED R010; verify sensor shunt marking
MAX_TEST_SECONDS = 10.0
DEFAULT_TEST_SECONDS = 3.0
SAMPLE_INTERVAL = 0.25
HIGH_CURRENT_MA = 500.0  # Informational only, NOT a safety cutoff
LOW_VOLTAGE_V = 2.0      # Informational only, NOT a safety cutoff

# Verified from user's gpioinfo output.
PINS = {"AIN1": ("gpiochip2", 8), "AIN2": ("gpiochip1", 33),
        "PWMA": ("gpiochip2", 16), "STBY": ("gpiochip1", 41)}

state_lock = threading.RLock()
state = {"running": False, "status": "Idle", "error": "", "session": 0,
         "start_time": None, "duration": DEFAULT_TEST_SECONDS, "last": None}
history = deque(maxlen=1200)
stop_event = threading.Event()
worker = None


def read_reg(reg):
    # i2cget returns a word with bytes reversed on this hardware.
    result = subprocess.run(["i2cget", "-y", str(I2C_BUS), hex(INA219_ADDRESS),
                             hex(reg), "w"], capture_output=True, text=True,
                            timeout=1.5, check=True)
    val = int(result.stdout.strip(), 16)
    return ((val & 0xff) << 8) | (val >> 8)


def read_sensor():
    bus_raw = read_reg(0x02)
    shunt_raw = read_reg(0x01)
    if shunt_raw >= 0x8000:
        shunt_raw -= 0x10000
    voltage = (bus_raw >> 3) * 0.004
    shunt_volts = shunt_raw * 0.00001
    current_ma = shunt_volts / SHUNT_OHMS * 1000
    power_w = voltage * current_ma / 1000
    flags = []
    if voltage < LOW_VOLTAGE_V:
        flags.append("LOW VOLTAGE")
    if abs(current_ma) > HIGH_CURRENT_MA:
        flags.append("HIGH CURRENT")
    return {"voltage": round(voltage, 3), "current": round(current_ma, 1),
            "power": round(power_w, 3), "shunt_mv": round(shunt_volts * 1000, 3),
            "warnings": flags}


class MotorGPIO:
    def __init__(self):
        self.chips = {}
        self.lines = {}
        try:
            for label, (chipname, offset) in PINS.items():
                if chipname not in self.chips:
                    self.chips[chipname] = gpiod.Chip(chipname)
                line = self.chips[chipname].get_line(offset)
                line.request(consumer="ensc351_dashboard",
                             type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
                self.lines[label] = line
        except Exception:
            self.close()
            raise

    def start(self):
        self.lines["AIN1"].set_value(1)
        self.lines["AIN2"].set_value(0)
        self.lines["PWMA"].set_value(1)
        self.lines["STBY"].set_value(1)

    def stop(self):
        # STBY LOW first, then all other lines LOW.
        for name in ("STBY", "PWMA", "AIN1", "AIN2"):
            if name in self.lines:
                try:
                    self.lines[name].set_value(0)
                except Exception:
                    pass

    def close(self):
        self.stop()
        for line in self.lines.values():
            try:
                line.release()
            except Exception:
                pass
        for chip in self.chips.values():
            try:
                chip.close()
            except Exception:
                pass


def run_test(session, seconds):
    motor = None
    started = time.monotonic()
    outcome = "Complete"
    problem = ""
    try:
        # Verify the INA219 responds BEFORE enabling the motor.
        read_reg(0x00)
        motor = MotorGPIO()
        if stop_event.is_set():
            outcome = "Stopped"
            return
        motor.start()
        started = time.monotonic()
        while time.monotonic() - started < seconds:
            if stop_event.is_set():
                outcome = "Stopped"
                break
            measurement = read_sensor()
            measurement["elapsed"] = round(time.monotonic() - started, 2)
            measurement["timestamp"] = datetime.now().isoformat(timespec="seconds")
            with state_lock:
                if state["session"] == session:
                    history.append(measurement)
                    state["last"] = measurement
            # Stop signal interrupts the wait promptly.
            if stop_event.wait(SAMPLE_INTERVAL):
                outcome = "Stopped"
                break
    except Exception as exc:
        outcome = "Error"
        problem = str(exc)
    finally:
        if motor:
            motor.close()
        with state_lock:
            if state["session"] == session:
                state["running"] = False
                state["status"] = outcome
                state["error"] = problem


HTML = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ENSC 351 | Motor Health</title>
<style>
:root{color-scheme:dark;font-family:Inter,ui-sans-serif,system-ui,Arial;background:#0b1020;color:#edf2ff}
*{box-sizing:border-box}body{margin:0}main{max-width:1120px;margin:auto;padding:30px 20px 50px}
header{display:flex;align-items:center;justify-content:space-between;gap:18px;flex-wrap:wrap;margin-bottom:26px}
.eyebrow{color:#94a7d6;font-weight:700;letter-spacing:.17em;font-size:.73rem}h1{font-size:clamp(1.6rem,4vw,2.5rem);letter-spacing:-.045em;margin:8px 0 2px}
p{color:#94a0bc;margin:6px 0}.pill{background:#1d2945;border:1px solid #334365;border-radius:40px;padding:9px 14px;font-weight:700;font-size:.85rem}.pill.good{color:#74e3ae}.pill.bad{color:#ff9c9c}
.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}.card,.panel{border:1px solid #2b3855;border-radius:17px;background:#141c2e;padding:22px}.card small{color:#aab6d1;font-size:.84rem}.value{font-size:clamp(1.7rem,3vw,2.9rem);font-weight:750;letter-spacing:-.06em;margin-top:14px}.value span{font-size:.46em;color:#91a4c9;letter-spacing:0}.layout{display:grid;grid-template-columns:1.75fr 1fr;gap:15px;margin-top:15px}.panel h2{font-size:1.05rem;margin:0 0 16px}.legend{display:flex;gap:18px;flex-wrap:wrap;color:#a7b6d3;font-size:.8rem;margin:10px 0}.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px}.chart{width:100%;height:260px;display:block;background:#10182a;border-radius:10px}.controls{display:flex;flex-direction:column;gap:14px}label{font-size:.87rem;color:#c8d3ea}input{width:100%;padding:12px;background:#0c1424;color:white;border:1px solid #415273;border-radius:9px;font-size:1rem;margin-top:7px}button,a.btn{font:inherit;font-weight:700;border:0;border-radius:10px;cursor:pointer;padding:13px 16px;text-align:center;text-decoration:none}button:disabled{opacity:.45;cursor:not-allowed}.start{background:#57d9a0;color:#062519}.stop{background:#ff8290;color:#2b0910}.download{background:#263654;color:#eaf0ff;display:block}.warn{margin-top:15px;background:#35291d;border:1px solid #645036;color:#ffd49d;padding:13px;border-radius:10px;font-size:.83rem}.foot{font-size:.82rem;color:#96a6c5;margin-top:18px}.error{color:#ff9c9c;min-height:22px;margin-top:8px;overflow-wrap:anywhere}.muted{font-size:.8rem} .updates{margin-top:16px;color:#9eacc9;font-size:.83rem}
@media(max-width:730px){.grid{grid-template-columns:1fr}.layout{grid-template-columns:1fr}.chart{height:210px}.card{padding:17px}.value{margin-top:5px}}
</style></head><body><main>
<header><div><div class="eyebrow">BEAGLEY-AI · ENSC 351</div><h1>Motor Health Monitor</h1><p>Live electrical telemetry · INA219 + TB6612FNG</p></div><div id="state" class="pill">● CONNECTING</div></header>
<section class="grid"><div class="card"><small>BUS VOLTAGE</small><div class="value"><b id="v">—</b> <span>V</span></div></div><div class="card"><small>ESTIMATED CURRENT</small><div class="value"><b id="i">—</b> <span>mA</span></div></div><div class="card"><small>ESTIMATED POWER</small><div class="value"><b id="p">—</b> <span>W</span></div></div></section>
<div class="layout"><section class="panel"><h2>Live measurements</h2><div class="legend"><span><i class="dot" style="background:#83b8ff"></i>Voltage (V)</span><span><i class="dot" style="background:#f4bd76"></i>Current (mA)</span></div><canvas id="chart" class="chart" aria-label="Recent voltage and current measurements" role="img"></canvas><div id="sample" class="updates">No measurements yet.</div></section>
<section class="panel"><h2>Motor controls</h2><div class="controls"><label for="duration">Test duration (1–10 seconds)<input id="duration" type="number" min="1" max="10" step="1" value="3"></label><button class="start" id="start" type="button">▶ Start motor test</button><button class="stop" id="stop" type="button" disabled>■ Stop motor</button><a class="btn download" href="/api/csv">↓ Download test CSV</a></div><div id="error" class="error" role="alert"></div><div class="warn">⚠ Electrical fault thresholds are informational, not protective. Disconnect motor power physically for an emergency. Verify shunt resistance (assumed 0.01 Ω) and secure battery wiring.</div></section></div>
<p class="foot">Current and power are estimates until the shunt resistance is verified. RPM, vibration and stall detection will be added when the encoder and accelerometer are connected. Local USB network only.</p>
</main><script>
const el=id=>document.getElementById(id), ctx=el('chart').getContext('2d');let previousSession=null;
function draw(points){let c=el('chart'),dpr=window.devicePixelRatio||1,w=c.clientWidth,h=c.clientHeight;c.width=Math.max(1,w*dpr);c.height=Math.max(1,h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);ctx.strokeStyle='#26334c';ctx.lineWidth=1;for(let k=1;k<=4;k++){let y=k*h/5;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke()}
if(points.length<2){ctx.fillStyle='#8e9cb9';ctx.font='14px system-ui';ctx.fillText('Chart fills during a motor test',18,h/2);return}
const series=[['voltage','#83b8ff'],['current','#f4bd76']];for(let [field,color] of series){const nums=points.map(p=>Number(p[field]));let lo=Math.min(...nums),hi=Math.max(...nums);let pad=Math.max((hi-lo)*.15,field==='voltage'?.05:10);lo-=pad;hi+=pad;ctx.beginPath();nums.forEach((v,j)=>{let x=12+(w-24)*j/(nums.length-1),y=12+(h-24)*(1-(v-lo)/(hi-lo));j?ctx.lineTo(x,y):ctx.moveTo(x,y)});ctx.strokeStyle=color;ctx.lineWidth=2;ctx.stroke()}}
async function refresh(){try{let r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('Unable to read board');let d=await r.json();el('state').textContent='● '+(d.running?'RUNNING':d.status.toUpperCase());el('state').className='pill '+(d.running?'good':d.status==='Error'?'bad':'');el('start').disabled=d.running;el('stop').disabled=!d.running;el('error').textContent=d.error||'';if(d.last){el('v').textContent=d.last.voltage.toFixed(3);el('i').textContent=d.last.current.toFixed(1);el('p').textContent=d.last.power.toFixed(3);el('sample').textContent=`${d.samples.length} samples · ${d.last.elapsed.toFixed(2)} s · ${d.last.warnings.length?d.last.warnings.join(', '):'No threshold warnings'}`;el('sample').style.color=d.last.warnings.length?'#ffb7a3':'#9eacc9'}else{el('v').textContent=el('i').textContent=el('p').textContent='—';el('sample').textContent='No measurements yet.'}draw(d.samples)}catch(e){el('state').textContent='● DISCONNECTED';el('state').className='pill bad';el('error').textContent=e.message;el('start').disabled=true;el('stop').disabled=true}}
async function action(path,body={}){try{let r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}),d=await r.json();if(!r.ok)throw Error(d.error||'Request failed');await refresh()}catch(e){el('error').textContent=e.message}}
el('start').addEventListener('click',()=>{let n=Number(el('duration').value);if(!Number.isFinite(n)||n<1||n>10){el('error').textContent='Set a duration between 1 and 10 seconds.';return}action('/api/start',{duration:n})});el('stop').addEventListener('click',()=>action('/api/stop'));window.addEventListener('resize',()=>refresh());refresh();setInterval(refresh,500);
</script></body></html>'''


class Handler(BaseHTTPRequestHandler):
    def send_bytes(self, data, content_type, status=200, filename=None):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        if filename:
            self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
        self.end_headers()
        self.wfile.write(data)

    def reply(self, obj, status=200):
        self.send_bytes(json.dumps(obj).encode("utf-8"), "application/json", status)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/":
            return self.send_bytes(HTML.encode("utf-8"), "text/html; charset=utf-8")
        if path == "/api/status":
            with state_lock:
                snapshot = dict(state)
                snapshot["samples"] = list(history)[-120:]
            return self.reply(snapshot)
        if path == "/api/csv":
            with state_lock:
                rows = list(history)
            out = io.StringIO()
            writer = csv.writer(out)
            writer.writerow(["timestamp", "elapsed_s", "voltage_v", "current_ma", "power_w", "shunt_mv", "warnings"])
            for x in rows:
                writer.writerow([x["timestamp"], x["elapsed"], x["voltage"], x["current"], x["power"], x["shunt_mv"], "; ".join(x["warnings"])])
            return self.send_bytes(out.getvalue().encode("utf-8"), "text/csv; charset=utf-8", filename="motor_data.csv")
        self.reply({"error": "Not found"}, 404)

    def do_POST(self):
        global worker
        path = urlparse(self.path).path
        if path not in ("/api/start", "/api/stop"):
            return self.reply({"error": "Not found"}, 404)
        n = int(self.headers.get("Content-Length", "0"))
        if n > 2048 or n < 0:
            return self.reply({"error": "Invalid request size"}, 400)
        try:
            body = json.loads(self.rfile.read(n) or b"{}")
        except (ValueError, UnicodeError):
            return self.reply({"error": "Invalid JSON"}, 400)
        with state_lock:
            if path == "/api/stop":
                stop_event.set()
                return self.reply({"ok": True, "message": "Stop requested"})
            if state["running"]:
                return self.reply({"error": "A test is already running"}, 409)
            try:
                duration = float(body.get("duration", DEFAULT_TEST_SECONDS))
                if not (1 <= duration <= MAX_TEST_SECONDS):
                    raise ValueError()
            except (TypeError, ValueError):
                return self.reply({"error": "Duration must be 1–10 seconds"}, 400)
            stop_event.clear()
            history.clear()
            state.update({"running": True, "status": "Running", "error": "", "last": None,
                          "session": state["session"] + 1, "duration": duration,
                          "start_time": datetime.now().isoformat(timespec="seconds")})
            worker = threading.Thread(target=run_test, args=(state["session"], duration), daemon=False)
            worker.start()
            self.reply({"ok": True})

    def log_message(self, fmt, *args):
        pass


def main():
    print(f"Motor dashboard: http://{HOST}:{PORT}")
    print("Private USB network only. Press Ctrl+C to stop the server.")
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    try:
        server.serve_forever(poll_interval=0.1)
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        stop_event.set()
        server.server_close()
        with state_lock:
            running_worker = worker
        if running_worker is not None:
            running_worker.join(timeout=5)
        print("Server stopped. Disconnect battery power when finished.")


if __name__ == "__main__":
    main()
