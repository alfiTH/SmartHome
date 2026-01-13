
import asyncio
import os
from dotenv import load_dotenv
from bleak import BleakScanner
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

load_dotenv()

def load_sensors():
    sensors = {}
    raw = os.getenv("SENSORS", "")

    for entry in raw.split(";"):
        if not entry:
            continue
        name, mac, type_sensor = entry.split(",")
        sensors[mac.upper()] = {
            "name": name,
            "type": type_sensor,
        }
    return sensors
SENSORS = load_sensors()


# -------- Influx --------
INFLUX_URL = "http://localhost:8086"
INFLUX_TOKEN = os.getenv("INFLUXDB_TOKEN")
INFLUX_ORG = os.getenv("INFLUXDB_ORG")
INFLUX_BUCKET = os.getenv("INFLUXDB_BUCKET")

client = InfluxDBClient(
    url=INFLUX_URL,
    token=INFLUX_TOKEN,
    org=INFLUX_ORG
)

write_api = client.write_api(write_options=SYNCHRONOUS)

def parse_payload(data):
    temp = int.from_bytes(data[0:2], 'big', signed=True) / 100
    hum  = int.from_bytes(data[2:4], 'big') / 100
    aqi  = data[4]
    tvoc = int.from_bytes(data[5:7], 'big')
    eco2 = int.from_bytes(data[7:9], 'big')
    batt = int.from_bytes(data[9:11], 'big')

    return temp, hum, aqi, tvoc, eco2, batt

async def detection_callback(device, advertisement_data):
    print(device.address)
    sensor = SENSORS.get(device.address)
    if  sensor is None:
        return
    mfg = advertisement_data.manufacturer_data
    print(mfg)
    for _, data in mfg.items():
        print(len(data))
        
        if sensor["type"]=="temperature-humidity-CO2" and len(data) == 12:
            temp, hum, aqi, tvoc, eco2, batt = parse_payload(data)

            point = (
                Point(sensor["name"])
                .field("temperature", temp)
                .field("humidity", hum)
                .field("aqi", aqi)
                .field("tvoc", tvoc)
                .field("eco2", eco2)
                .field("battery", batt)
            )

            write_api.write(bucket=INFLUX_BUCKET, record=point)
            print("Guardado:", point)

async def main():
    scanner = BleakScanner(detection_callback)
    await scanner.start()
    await asyncio.sleep(999999)  # correr indefinidamente

asyncio.run(main())
