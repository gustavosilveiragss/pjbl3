# Flask
ADDRESS = "127.0.0.1"
PORT = 5000

# MQTT
BROKER_URL = "broker.emqx.io"
BROKER_PORT = 1883
BROKER_KEEPALIVE = 60
BROKER_TLS_ENABLED = False

TOP_PERMISSION_STATE = "PERMISSION_STATE"
TOP_IR_STATE = "IR_STATE"
TOP_PASSWORD = "PASSWORD"
TOP_FREQUENCY = "FREQUENCY"
TOP_TEMPERATURE = "TEMPERATURE"
TOP_HUMIDITY = "HUMIDITY"
TOP_DEVICE = "DEVICE"
TOP_USER = "USER"

REQUEST = "REQ"
READ = "R"
WRITE = "W"
DELETE = "D"
RESPONSE = "RES"
CRUD = "CRUD"

RESPONSE_OK = '{"status": "OK"}'
