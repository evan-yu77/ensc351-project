# Python web server that links embedded C code to the web dashboard 


import asyncio
import socket 
import json 
import re 
from fastapi import FastAPI, WebSocket, WebSocketDisconnect 
from fastapi.staticfiles import StaticFiles 
import uvicorn

app = FastAPI()

# udp config for C bg daemon

UDP_IP = "127.0.0.1"
UDP_PORT = 12345
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setblocking(False)

# keep track of connected browser tabs
active_connections = set()

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    active_connections.add(websocket)
    try: 
        while True:
            data = await websocket.receive_text()
            command = json.loads(data)
            cmd_type = command.get('command')

            if cmd_type == 'speed':
                cmd_str = f"speed {command['value']}"
                sock.sendto(cmd_str.encode(), (UDP_IP, UDP_PORT))
            elif cmd_type == 'reset':
                sock.sendto(b"reset", (UDP_IP, UDP_PORT))
            elif cmd_type == 'fault_toggle':
                cmd_str = "fault on" if command.get('enabled') else "fault off"
                sock.sendto(cmd_str.encode(), (UDP_IP, UDP_PORT))
    except WebSocketDisconnect:
        active_connections.remove(websocket)

async def udp_poller():
    while True:
        # ask C daemon for data 
        sock.sendto(b"?", (UDP_IP, UDP_PORT))
        await asyncio.sleep(0.1) # 10 Hz polling rate
        
        try:
            # read the response string
            data, _ = sock.recvfrom(1024)
            text = data.decode('utf-8').strip()
            
            # parse the C string into variables using Regex
            match = re.search(
                r"Accel X:\s*(-?\d+),\s*Accel Y:\s*(-?\d+),\s*Current \(mA\):\s*(-?\d+(?:\.\d+)?),\s*RPM:\s*(-?\d+),\s*Speed:\s*(-?\d+),\s*Fault:\s*(\d+)",
                text
            )            
            if match:
                fault_code = int(match.group(6))
                payload = {
                    "accel_x": int(match.group(1)),
                    "accel_y": int(match.group(2)),
                    "current_ma": float(match.group(3)),
                    "rpm": int(match.group(4)),
                    "speed": int(match.group(5)),
                    "fault_code": fault_code,
                    "fault": fault_code > 0
                }
                
                # blast the JSON to all open web dashboards
                for connection in list(active_connections):
                    await connection.send_json(payload)
        
        except BlockingIOError:
            pass # no UDP packet arrived yet, just skip
        except Exception as e:
            print(f"Polling error: {e}")

@app.on_event("startup")
async def startup_event():
    asyncio.create_task(udp_poller())


app.mount("/", StaticFiles(directory="website", html=True), name="website")

if __name__ == "__main__":
    print("starting web bridge on http://0.0.0.0:8080")
    uvicorn.run(app, host="0.0.0.0", port=8080)
