#!/bin/bash
# Console MQTT do projeto PNAAT
exec /home/gilvantws/.espressif/python_env/idf5.4_py3.12_env/bin/python \
    "$(dirname "$0")/pnaat_mqtt.py" "$@"
