#!/usr/bin/env python3
import json
import random
import sys
import threading

import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORTA = 1883
BASE = "testtopic/pnaat"
TOP_STATUS = f"{BASE}/status"
TOP_CMD = f"{BASE}/cmd"

COMANDOS = {"on", "off", "up", "down"}

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                client_id=f"pnaat-console-{random.randint(1000, 9999)}")

imprimindo = threading.Lock()
conectou = threading.Event()
encerrando = False


def log(txt):
    with imprimindo:
        print(txt)


def bonito(payload):
    try:
        d = json.loads(payload)
        return (f"{d.get('hora','??:??:??')} | "
                f"{d.get('fonte','?'):<12} | LED {'ON ' if d.get('led') else 'OFF'} | "
                f"brilho {d.get('brilho','?')}%")
    except (json.JSONDecodeError, UnicodeDecodeError):
        return f"(bruto) {payload}"


def on_connect(c, u, flags, rc, props=None):
    log(f"[ok] Conectado ao broker {BROKER}")
    c.subscribe(TOP_STATUS, qos=1)
    log(f"[ok] Ouvindo: {TOP_STATUS}")
    log(f"[ok] Envie comandos com: {', '.join(sorted(COMANDOS))} (ou 'ajuda', 'sair')\n")
    conectou.set()


def on_disconnect(c, u, flags, rc, props=None):
    if not encerrando:
        log("[!] Desconectado do broker (tentando reconectar...)")


def on_message(c, u, msg):
    txt = msg.payload.decode(errors="replace")
    if msg.topic == TOP_CMD and txt in COMANDOS:
        return
    log(f"  PLACA >> {bonito(txt)}")


c.on_connect = on_connect
c.on_disconnect = on_disconnect
c.on_message = on_message
c.reconnect_delay_set(1, 30)
c.connect(BROKER, PORTA, 30)
c.loop_start()

print("=" * 60)
print(" PNAAT - console MQTT (Ctrl+C ou 'sair' para encerrar)")
print("=" * 60)

if not conectou.wait(15):
    print("[!] Nao conectou ao broker em 15 s. Verifique a internet.")
    c.loop_stop()
    sys.exit(1)

try:
    while True:
        cmd = input("comando> ").strip().lower()
        if not cmd:
            continue
        if cmd in ("sair", "exit", "quit"):
            break
        if cmd == "ajuda":
            print(f"  Comandos: {', '.join(sorted(COMANDOS))} | sair | ajuda")
            continue
        if cmd in COMANDOS:
            info = c.publish(TOP_CMD, cmd, qos=1)
            info.wait_for_publish()
            log(f"  VOCE  >> '{cmd}' enviado para {TOP_CMD}")
        else:
            print(f"  [?] Comando desconhecido: '{cmd}' ('ajuda' lista os validos)")
except (KeyboardInterrupt, EOFError):
    pass
finally:
    encerrando = True
    c.disconnect()
    c.loop_stop()
    print("\n[ok] Console encerrado")
